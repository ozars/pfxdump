#!/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt

plt.rcParams["font.size"] = 12

df = pd.read_csv("results.csv")
df.at[df.chunks == 0, 'chunks'] = 1
df = df.groupby("chunks").mean().reset_index()
df['size'] /= 1024 * 1024

print(df.to_string())

plt.plot(df['chunks'], df['time'], "o")
plt.grid(linestyle=':', zorder=0)
plt.ylabel("Time (sec)")
plt.xlabel("Number of threads")
plt.tight_layout()
plt.savefig("multithread.eps")
#  plt.show()
