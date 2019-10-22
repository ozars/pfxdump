import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

def draw(df):
    df.prefix = pd.Categorical(df.prefix, ordered=True, categories=df.prefix.unique())
    df.prefix = df.prefix.cat.codes
    df = df.groupby(['with_zx', 'prefix'])['time'].mean()
    df.groupby('with_zx').plot(x='prefix', y='time')
    rng = list(range(0, 101, 5))
    plt.xticks(rng, rng)
    plt.show()

df = pd.read_csv(sys.argv[1])
df_remote = df[df['remote'] == 1]
df_local = df[df['remote'] == 0]
draw(df_remote)
draw(df_local)
