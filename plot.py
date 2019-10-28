import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

def draw_time(df):
    df.prefix = pd.Categorical(df.prefix, ordered=True, categories=df.prefix.unique())
    df.prefix = df.prefix.cat.codes
    df.time = df.time.astype(dtype=float)
    df = df.groupby(['with_zx', 'prefix'])['time'].mean()
    df.groupby('with_zx').plot(x='prefix', y='time')
    rng = list(range(0, 101, 5))
    plt.xticks(rng, rng)
    plt.xlabel("Prefix")
    plt.ylabel("Time (sec)")
    plt.legend(["Without ZIDX", "With ZIDX"])
    plt.show()

def draw_data(df):
    df.prefix = pd.Categorical(df.prefix, ordered=True, categories=df.prefix.unique())
    df.prefix = df.prefix.cat.codes
    df.rx = df.rx.astype(dtype=float) / 1024.0 / 1024.0
    df = df.groupby(['with_zx', 'prefix'])['rx'].mean()
    df.groupby('with_zx').plot(x='prefix', y='rx')
    rng = list(range(0, 101, 5))
    plt.xticks(rng, rng)
    plt.xlabel("Prefix")
    plt.ylabel("Data Received (MB)")
    plt.legend(["Without ZIDX", "With ZIDX"])
    plt.show()

df = pd.read_csv(sys.argv[1])
df_remote = df[df['remote'] == 1]
df_local = df[df['remote'] == 0]
draw_data(df_remote)
draw_time(df_remote)
draw_time(df_local)
