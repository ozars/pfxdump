#!/usr/bin/env python3

import re
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

plt.rcParams["font.size"] = 12

from pathlib import Path

Path.__add__ = lambda self, val: Path(str(self) + val)

plot_type, input_path, output_name = sys.argv[1:]

def pretty_size(size):
    if type(size) is str:
        size = int(size)
    if size < 1024:
        return f'{size}KB'
    return f'{str(round(size/1024.0, 2)).rstrip("0").rstrip(".")}MB'

if plot_type == 'csv':
    df = pd.read_csv(input_path)
    df = df[df.comp_state != 'uncomp']

    df.prefix = pd.Categorical(df.prefix, ordered=True, categories=df.prefix.unique())
    df['prefix_idx'] = df.prefix.cat.codes

    df.time = df.time.apply(pd.to_numeric, errors="coerce")
    df.data = df.data.apply(pd.to_numeric, errors="coerce")
    df = df[~df.mrt_file.str.contains('rrc21')]

    print(f'Started preprocessing {len(df)} lines')

    for col in ['time', 'data', 'status']:
        if col is 'status':
            err = ~df[col].isna() & df[col].str.contains('error')
        else:
            err = df[col].isna()
        print(f'Filtering out {len(df[err])} columns for "{col}"')
        df = df[~err]

    print(f'Started plotting remaining {len(df)} datapoints')

    df.data = df.data.astype(float)/1024/1024

    dfg = df.groupby(['with_zx', 'span', 'prefix_idx']).mean().groupby(['with_zx', 'span'])

    def time_graph(df, ylabel, zoom_in, fname, clear=True):
        def label(k):
            if k[0] == 0:
                return 'Without ZIDX'
            assert k[0] == 1
            return f'With ZIDX (span: {pretty_size(k[1])})'

        if clear:
            plt.clf()
        colors = iter([f'C{i}' for i in range(10)])
        plt.grid(linestyle=':')
        plt.ylabel(ylabel)
        plt.xlabel("Prefix")
        for k, v in df:
            if zoom_in and k[0] == 0:
                next(colors)
                continue
            plt.plot(v.values, label=label(k), color=next(colors))
        plt.legend()
        sz = df.size().unique()[0]
        rng = range(0, sz + 1, sz // 20)
        plt.xticks(rng)
        bottom, top = plt.ylim()
        plt.xlim(rng[0], rng[-1])
        plt.ylim(bottom, top * 1.05)
        plt.tight_layout()
        plt.savefig(fname + ".eps")

    time_graph(dfg['time'], "Time (sec)", zoom_in=False, fname=output_name + "_time")
    if len(dfg) > 2:
        time_graph(dfg['time'], "Time (sec)", zoom_in=True, fname=output_name + "_time_spans")
    if len(df['data'].unique()) != 1:
        time_graph(dfg['data'], "Data Transmitted (MB)", zoom_in=False, fname=output_name + "_data")

elif plot_type == 'zx':
    p = Path(input_path)
    ds = []
    r = re.compile(r'.*/(rrc\d{2})/\d{4}.\d{2}/bview.(\d{4})(\d{2})(\d{2}).(\d{4}).gz_(\d+)_comp.zx')
    for f in p.glob('**/*.zx'):
        m = r.match(str(f))
        if m:
            g = m.groups()
            d = dict(zip(['rrc', 'year', 'month', 'day', 'time', 'span'], g))
            d['span'] = int(d['span'])
            d['size'] = f.stat().st_size / 1024 / 1024
            gzf = f.parent / "bview.{year}{month}{day}.{time}.gz".format(**d)
            d['org_size'] = gzf.stat().st_size / 1024 / 1024
            d['comp_size'] = (f + ".gz").stat().st_size / 1024 / 1024
            d['ratio'] = d['size'] / d['org_size'] * 100
            d['comp_ratio'] = d['comp_size'] / d['org_size'] * 100
            ds.append(d)
    df = pd.DataFrame(ds)
    dfg = df.groupby('span')

    def relative_coord(ax, rect, coord):
        return (
            ax.transData.transform_point(
                    [rect.get_x() + rect.get_width() / 2,
                        rect.get_height()])
                + ax.transAxes.transform_point(coord)
                - ax.transAxes.transform_point([0, 0])
            )

    def zx_bar(dfg, ylabel, fname, color, clear=True):
        if clear:
            plt.clf()
        fig, ax = plt.subplots()
        rng = range(len(dfg))
        rect = plt.bar(rng, dfg.mean(), color=color, zorder=3)
        for r in rect:
            height = r.get_height()
            ax.text(*relative_coord(ax, r, [0, -.03]), f'{height:.2f}%',
                    ha='center', va='center', color='#EFEFEF', fontweight='bold', transform=None)
        ax.grid(linestyle=':', zorder=0)
        ax.set_ylabel(ylabel)
        ax.set_xlabel("Spans")
        ax.set_xticks(rng)
        ax.set_xticklabels(dfg.groups.keys())
        fig.savefig(fname + ".eps")

    def zx_single_bar(dfgs, labels, ylabel, fname, clear=True):
        if clear:
            plt.clf()
        fig, ax = plt.subplots()
        rng = np.arange(len(dfgs[0]))
        width = 0.45;
        rects = []
        for i in range(len(dfgs)):
            dfg = dfgs[i]
            rect = ax.bar(rng - width / 2 + (width * i), dfg.mean(), width, label=labels[i], zorder=3)
            #  for r in rect:
            #      height = r.get_height()
            #      ax.text(*relative_coord(ax, r, [0, +.03]), f'{height:.2f}%',
            #              ha='center', va='center', color='black', fontweight='bold', transform=None)
            for r in rect:
                height = r.get_height()
                ax.annotate(f'{height:.2f}%', xy=(r.get_x() + r.get_width() / 2, height),
                            xytext=(0, 3), textcoords="offset points", ha='center', va='bottom')
            assert dfg.groups.keys() == dfgs[0].groups.keys()
            assert len(dfg) == len(dfgs[0])
        ax.grid(linestyle=':', zorder=0)
        ax.legend()
        ax.set_ylabel(ylabel, fontsize=12)
        ax.set_xlabel("Spans", fontsize=12)
        ax.set_xticks(rng)
        ax.set_xticklabels([pretty_size(size) for size in dfgs[0].groups.keys()])
        bottom, top = ax.get_ylim()
        ax.set_ylim(bottom, top * 1.05)
        fig.tight_layout()
        fig.savefig(fname + ".eps")

    plt.rcParams["font.size"] = 10
    #  zx_bar(dfg['ratio'], "ZIDX File Size Ratio (%)", output_name + "_zx_ratio", "C0")
    #  zx_bar(dfg['comp_ratio'], "Compressed ZIDX File Size Ratio (%)", output_name + "_zx_comp_ratio", "C1")
    zx_single_bar([dfg['ratio'], dfg['comp_ratio']], ["Not compressed", "Compressed"], "ZIDX File Size Ratio (%)", output_name + "_zx_single")
