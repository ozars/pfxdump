#!/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("results.csv")
df.at[df.chunks == 0, 'chunks'] = 1
df = df.groupby("chunks").mean().reset_index()

print(df)

plt.plot(df['chunks'], df['time'], "o")
plt.grid(linestyle=':', zorder=0)
plt.ylabel("Time")
plt.xlabel("Threads")
plt.tight_layout()
plt.show()
plt.savefig("multithread.png")
