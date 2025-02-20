import pandas as pd
import json
import pathlib
import seaborn as sns
import matplotlib.pyplot as plt

sns.set_theme()

script_dir = pathlib.Path(__file__).parent
df = pd.DataFrame.from_records(json.load(open(script_dir / "out_qlen.txt")))

sns.relplot(
  df,
  kind='line',
  x="time", y="bytes", hue="iface",
  row="node",
  palette='plasma', aspect=4, height=2.5, alpha=0.7, style='iface',
  facet_kws={'sharey': False, 'sharex': False})

plt.tight_layout()
plt.show()

df = pd.DataFrame.from_records(json.load(open(script_dir / "out_allgather-miss.json")))
print(df)

sns.relplot(
  df,
  kind='scatter',
  x="chunk", y="block", s=30,
  linewidth=0 # Avoid outline when there is a lot of points
)

stats = json.load(open(script_dir / "out_allgather-stats.json"))
nodes = stats["block_count"]
mtu = stats["chunk_size"]
chunk_count = stats["total_chunk_count"]

ax = plt.gca()
ax.set_ylim(-0.5, nodes - 0.5)
ax.set_xlim(-0.5, chunk_count + 0.5)

for i in range(nodes):
  x1 = chunk_count / nodes * i
  x2 = x1
  plt.axline((x1, 0), (x2, nodes), linestyle="--", color="r")

from matplotlib.ticker import MaxNLocator
import matplotlib.ticker as ticker

ax.yaxis.set_major_locator(MaxNLocator(integer=True))
ax.xaxis.set_major_locator(ticker.MultipleLocator(chunk_count / nodes))

plt.tight_layout()
plt.show()