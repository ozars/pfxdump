#!/usr/bin/env python3

import argparse
import datetime
import logging
import os
import re
import shutil
import subprocess

import pandas as pd
import matplotlib.pyplot as plt

from dateutil.parser import parse as parse_date
from pathlib import Path

logger = logging.getLogger(__name__)
cwd = Path(__file__).parent

zidx_bin = cwd / "bin" / "zidx"
pfxdump_bin = cwd / "bin" / "pfxdump"

data_dir = cwd / "data"
experiment_dir = cwd / "experiment"

sample_prefixes_path = data_dir / "sample_prefixes"

Path.__add__ = lambda self, val: Path(str(self) + val)

def _date_mod(time, delta, epoch=None):
    if epoch is None:
        epoch = datetime.datetime(1970, 1, 1, tzinfo=time.tzinfo)
    return (time - epoch) % delta

def _date_round(time, delta, epoch=None):
    mod = _date_mod(time, delta, epoch)
    if mod < (delta / 2):
       return time - mod
    return time + (delta - mod)

def _date_ceil(time, delta, epoch=None):
    mod = _date_mod(time, delta, epoch)
    if mod == datetime.timedelta():
        return time
    return time + (delta - mod)

def _date_floor(time, delta, epoch=None):
    mod = _date_mod(time, delta, epoch)
    return time - mod

def _date_range(start, end, delta):
    _ZERO_TIMEDELTA = datetime.timedelta()
    if delta == _ZERO_TIMEDELTA:
        raise ValueError("delta argument cannot be zero in date_range")
    if delta > _ZERO_TIMEDELTA:
        while start < end:
            yield start
            start += delta
    else:
        while start > end:
            yield start
            start += delta

def _arg_split(c):
    def _arg_split_func(s):
        return s.split(c)
    return _arg_split_func

def _arg_date_range(s):
    s = s.split(",")
    if len(s) != 2:
        raise ValueError(
                "Date range should be splitted with a single ',' character.")
    from_date, to_date = parse_date(s[0]), parse_date(s[1])
    return (from_date, to_date)

def _mrt_path_from_date(d, ext=".gz"):
    return (
        Path(f"{d.year}.{d.month:02}") /
        f"bview.{d.year}{d.month:02}{d.day:02}.{d.hour:02}{d.minute:02}{ext}"
    )

def _iter_mrt_paths_from_date_range(rng):
    eight_hours = datetime.timedelta(hours=8)
    a_second = datetime.timedelta(seconds=1)
    rng = (_date_ceil(rng[0], eight_hours), _date_floor(rng[1], eight_hours))
    yield from (_mrt_path_from_date(d)
                    for d in _date_range(
                                    rng[0], rng[1] + a_second, eight_hours))

def _mrt_path_range(date_range, collectors, base_path, is_output=False):
    for p in _iter_mrt_paths_from_date_range(date_range):
        for c in collectors:
            path =  base_path / c
            if is_output:
                logger.info(f"Creating directory '{path}' if not exists...")
                path.mkdir(parents=True, exist_ok=True)
            assert path.exists()
            assert path.is_dir()

            path /= p
            if is_output and not p.exists():
                logger.info(f"Creating directory '{path.parent}' if not exists...")
                path.parent.mkdir(exist_ok=True)
            yield path

def _mrt_path_pairs_range(date_range, collectors, from_path, to_path):
    yield from zip(
            _mrt_path_range(date_range, collectors, from_path),
            _mrt_path_range(date_range, collectors, to_path, is_output=True))

def _escape_bash_arg(arg):
    arg = str(arg)
    if "\\" in arg or "\"" in arg:
        return '$"' + arg.replace("\\", "\\\\").replace("\"", "\\\"") + '"'
    return arg

def _run(cmd_args):
    logger.debug(f"Run: {cmd_args}")
    return subprocess.run(cmd_args, capture_output=True)

def _timed_run(cmd_args):
    cmd = " ".join(_escape_bash_arg(arg) for arg in cmd_args)
    logger.debug(f"Timed run: {cmd}")
    return subprocess.run(
                ["bash", "-c", "export TIMEFORMAT=%R; time " + cmd],
                capture_output=True)

def _read_sampled_prefixes(f):
    if not hasattr(f, read):
        f = open(str(f))
    for line in f:
        yield line.strip()

def _args_callback(func, arg_names):
    def _invoke_callback(parsed_args):
        return func(*[getattr(parsed_args, name) for name in arg_names])
    return _invoke_callback

def NoneOr(this_type):
    def get_type(t):
        if t is None:
            return None
        return this_type(t)
    return get_type

def PathOrUrl(s):
    if type(s) is str and (s.beginswith('http://') or s.beginswith('https://')):
        return s
    return Path(s)

def copy(date_range, collectors, from_path, to_path):
    for from_file, to_file in _mrt_path_pairs_range(
            date_range, collectors, from_path, to_path):
        logger.info(f"Copying '{from_file}' to '{to_file}'...")
        shutil.copy(from_file, to_file)

def run_zidx(*args):
    return _timed_run([str(zidx_bin), *args])

def run_pfxdump(*args):
    return _timed_run([str(pfxdump_bin), *args])

def zidx(date_range, collectors, from_path, to_path, spans, softlink_span):
    if to_path is None:
        to_path = from_path
    assert str(softlink_span) in spans
    for from_file, to_file in _mrt_path_pairs_range(
            date_range, collectors, from_path, to_path):
        for span in spans:
            logger.debug(run_zidx(from_file, to_file + f"_{span}_uncomp.zx", int(span)*1024, "1"))
            logger.debug(run_zidx(from_file, to_file + f"_{span}_comp.zx", int(span)*1024, "0"))
        symlink = to_file + ".zx"
        if symlink.exists():
            symlink.unlink()
        symlink.symlink_to((to_file + f"_{softlink_span}_comp.zx").name)

def sample_prefixes(
        date_range, collectors, gzip_path, output_path, num):
    r = re.compile(rb'debug: (.+?)\n')
    first_path = True
    order = {}
    for path in _mrt_path_range(date_range, collectors, gzip_path):
        p = _run([str(pfxdump_bin), str(path), "-", "255.255.255.255/32", "-i", "-d"])
        all_prefixes = r.finditer(p.stdout)
        if first_path:
            for prefix in all_prefixes:
                prefix = prefix.group(1)
                order[prefix] = len(order)
            s = set(order)
            first_path = False
        else:
            logger.debug("Size before: %d", len(s))
            s &= set(prefix.group(1) for prefix in all_prefixes)
            logger.debug("Size after: %d", len(s))

    s = sorted(list(s), key=lambda v: order[v])
    with open(output_path, "wb") as output:
        output.write(b"\n".join((s[len(s)//num*i] for i in range(num))))
    logger.debug(f"Sampled {num} prefixes to file '{output_path}'.")

def experiment(
        date_range, collectors, experiment_name, gzip_path, zidx_path,
        output_path, sample_prefixes_path, spans, without_zx):
    if zidx_path is None:
        zidx_path = gzip_path
    os.makedirs(output_path, exist_ok=True)
    with open(output_path / f"{experiment_name}.metadata", "w") as metadata:
        metadata.write("\n".join([
            datetime.datetime.now(),
            date_range,
            collectors,
            experiment_name,
            gzip_path,
            zidx_path,
            output_path,
            sample_prefixes_path,
            spans
        ])))

    pnf = re.compile(r"^([^\d]+)(\d+\.\d+)$")
    with open(sample_prefixes_path) as f:
        prefixes = [line.rstrip() for line in f]
    with open(output_path / f"{experiment_name}.csv", "w") as f:
        f.write(",".join(
            ["with_zx", "span", "comp_state", "prefix", "time", "data",
                "mrt_file", "status"]) + "\n")
        for gzip_file, zidx_file in _mrt_path_pairs_range(
                date_range, collectors, gzip_path, zidx_path):
            for p in prefixes:
                if without_zx:
                    time = run_pfxdump(str(gzip_file), "-", p, "-i")\
                                    .stderr.rstrip().decode('ascii')
                    m = pnf.match(time)
                    if m:
                        status = '"' + m.group(1).replace('"', '\\"') + '"'
                        time = m.group(2)
                    else:
                        status = ""
                    f.write(",".join(["0", "0", "none", p, time, "0", str(gzip_file), status]) + "\n")
                    logger.debug(f"Done: {p} on {gzip_file} without span.")
                for span in spans:
                    for comp_state in ["comp", "uncomp"]:
                        zx = zidx_file + f"_{span}_{comp_state}.zx"
                        time = run_pfxdump(str(gzip_file), str(zx), p)\
                                        .stderr.rstrip().decode('ascii')
                        m = pnf.match(time)
                        if m:
                            status = '"' + m.group(1).replace('"', '\\"') + '"'
                            time = m.group(2)
                        else:
                            status = ""
                        f.write(",".join(["1", span, comp_state, p, time, "0", str(gzip_file), status]) + "\n")
                        logger.debug(f"Done: {p} on {gzip_file} with span {span} {comp_state}.")

def experiment_plot(experiment_name, experiment_path, output_path, without_zx, show):
    if without_zx:
        agg = ['with_zx', 'span']
    else:
        agg = ['span']
    df = pd.read_csv(experiment_path / (experiment_name + ".csv"))
    df.prefix = pd.Categorical(df.prefix, ordered=True, categories=df.prefix.unique())

    df = df.astype({'time': 'double'})
    rng = list(range(0, len(df.prefix.unique()) + 1, len(df.prefix.unique()) // 20))
    if without_zx:
        df = df[df.comp_state != "uncomp"]
    else:
        df = df[df.comp_state == "comp"]
    df = df.groupby(agg + ['prefix'])['time'].agg('mean').groupby(agg)
    logger.info(df.describe())
    df.plot(x='prefix', y='time', legend=True)
    plt.xticks(rng, rng)
    plt.xlabel("Prefix")
    plt.ylabel("Time (sec)")
    if output_path:
        plt.savefig(str(output_path / (experiment_name + ".png")))
    if show:
        plt.show()

def main(args=None):
    default_collectors="rrc19 rrc11 rrc03 rrc10 rrc13 rrc15 rrc00 rrc12 rrc21 rrc20"
    default_date_range="2018/01/01-00:00,2018/01/07-16:00"
    default_date_range_one_day="2018/01/01-00:00,2018/01/01-16:00"
    default_date_range_one_snapshot="2018/01/01-00:00,2018/01/01-00:00"
    default_spans="1024,256,512,2048,4096"
    default_softlinked_span="1024"

    parser = argparse.ArgumentParser()
    parser.add_argument("-v", "--verbose", action="count")
    parser.add_argument(
            "-c", "--collectors", default=default_collectors,
            type=_arg_split(" "))
    subparsers = parser.add_subparsers()

    subparser = subparsers.add_parser("copy")
    subparser.add_argument("from_path", type=Path)
    subparser.add_argument("to_path", nargs="?", default=data_dir, type=Path)
    subparser.add_argument(
            "-d", "--date-range", default=default_date_range,
            type=_arg_date_range)
    subparser.set_defaults(
            func=_args_callback(copy,
                ["date_range", "collectors", "from_path", "to_path"]))

    subparser = subparsers.add_parser("zidx")
    subparser.add_argument("from_path", nargs="?", default=data_dir, type=Path)
    subparser.add_argument(
            "to_path", nargs="?", default=None, type=NoneOr(Path))
    subparser.add_argument(
            "-s", "--spans", default=default_spans, type=_arg_split(","))
    subparser.add_argument(
            "--softlink-span", default=default_softlinked_span)
    subparser.add_argument(
            "-d", "--date-range", default=default_date_range,
            type=_arg_date_range)
    subparser.set_defaults(
            func=_args_callback(zidx,
                ["date_range", "collectors", "from_path", "to_path", "spans",
                    "softlink_span"]))

    subparser = subparsers.add_parser("sample_prefixes")
    subparser.add_argument(
            "gzip_path", nargs="?", default=data_dir, type=PathOrUrl)
    subparser.add_argument(
            "output_path", nargs="?", default=sample_prefixes_path)
    subparser.add_argument("num", nargs="?", default=100, type=int)
    subparser.add_argument(
            "-d", "--date-range", default=default_date_range,
            type=_arg_date_range)
    subparser.set_defaults(
            func=_args_callback(sample_prefixes,
                ["date_range", "collectors", "gzip_path", "output_path",
                    "num"]))

    subparser = subparsers.add_parser("experiment")
    subparser.add_argument("experiment_name")
    subparser.add_argument(
            "gzip_path", nargs="?", default=data_dir, type=PathOrUrl)
    subparser.add_argument(
            "zidx_path", nargs="?", default=data_dir, type=Path)
    subparser.add_argument(
            "sample_prefixes_path", nargs="?", default=sample_prefixes_path,
            type=Path)
    subparser.add_argument("--output-path", default=experiment_dir)
    subparser.add_argument(
            "-s", "--spans", default=default_spans, type=_arg_split(","))
    subparser.add_argument("--without-zx", action="store_true")
    subparser.add_argument(
            "-d", "--date-range", default=default_date_range,
            type=_arg_date_range)
    subparser.set_defaults(
            func=_args_callback(experiment,
                ["date_range", "collectors", "experiment_name", "gzip_path",
                    "zidx_path", "output_path", "sample_prefixes_path",
                    "spans", "without_zx"]))

    subparser = subparsers.add_parser("experiment_plot")
    subparser.add_argument("experiment_name")
    subparser.add_argument("--experiment-path", default=experiment_dir, type=Path)
    subparser.add_argument("--output-path", default=experiment_dir, type=Path)
    subparser.add_argument("--without-zx", action="store_true")
    subparser.add_argument("--show", action="store_true")
    subparser.set_defaults(
            func=_args_callback(experiment_plot,
                ["experiment_name", "experiment_path", "output_path", "without_zx", "show"]))

    args = parser.parse_args(args)

    if args.verbose:
        logger.addHandler(logging.StreamHandler())
        if args.verbose == 1:
            logger.setLevel(logging.INFO)
        elif args.verbose >= 2:
            logger.setLevel(logging.DEBUG)

    args.func(args)

if __name__ == "__main__":
    main()
