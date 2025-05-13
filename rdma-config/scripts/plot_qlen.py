import pandas as pd
import json
import pathlib
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import matplotlib.ticker as ticker
from avro.io import DatumReader, DatumWriter
from avro.datafile import DataFileReader, DataFileWriter

script_dir = pathlib.Path(__file__).parent
img_dir = script_dir / "img"
img_dir.mkdir(exist_ok=True)

def savefig(name):
  plt.savefig(img_dir / f"{name}.png")

def main():
  sns.set_theme()
  plot_recv_chunks()

  df = pd.DataFrame.from_records(json.load(open(script_dir / "out_pfc.json")),
    columns=["time", "dev", "paused", "node"])
  print(df)
  sns.relplot(
    df,
    kind='scatter',
    x="time", y="dev",
    hue="paused", s=30,
    row="node",
    aspect=8,
    height=1,
    linewidth=0 # Avoid outline when there is a lot of points
  )

  ax = plt.gca()
  ax.yaxis.set_major_locator(MaxNLocator(integer=True))

  savefig("pfc")

  df = pd.DataFrame.from_records(json.load(open(script_dir / "out_qlen.json")))

  for y in ['egress', 'ingress']:
    sns.relplot(
      df,
      kind='line',
      x="time", y=y, hue="iface",
      row="node",
      linewidth=2,
      palette='brg', aspect=4, height=2.5, style='iface',
      alpha=0.75,
      legend=True,
      facet_kws={'sharey': False, 'sharex': False})
    plt.gca().set_ylim(bottom=0)
    plt.tight_layout()
    savefig(f"{y}")

  df = pd.DataFrame.from_records(
    json.load(open(script_dir / "out_allgather-miss.json")),
    columns=["block", "chunk"])
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

  ax.yaxis.set_major_locator(MaxNLocator(integer=True))
  ax.xaxis.set_major_locator(ticker.MultipleLocator(chunk_count / nodes))

  plt.tight_layout()
  plt.show()
  savefig("miss")

def scientific_axis(g):
   for axes in g.axes.flat:
      axes.ticklabel_format(axis='both', style='scientific', scilimits=(0, 0))


def read_avro(path) -> pd.DataFrame:
  with open(path, "rb") as file:
    reader = DataFileReader(file, DatumReader())
    return pd.DataFrame.from_records([r for r in reader])


def plot_recv_chunks():
  df = read_avro(script_dir / "out_recv_chunks.avro")
  print(df)
  g = sns.relplot(df, kind='scatter',
    x="time", y="chunk",
    col="node", col_wrap = 5,
    s=10,
    height=3,
    legend=True,
    facet_kws={'sharey': False, 'sharex': False})
  
  scientific_axis(g)
  plt.gca().set_ylim(bottom=0)
  plt.tight_layout()
  savefig("recv")

def plot_qp() -> None:
  df = pd.DataFrame.from_records(json.load(open(script_dir / "out_qp.json")))
  df["inflight"] = df["end"] - df["acked"]
  g = sns.relplot(
    df,
    kind='line',
    x="time", y="inflight", hue="key", style="key",
    col="node",
    linewidth=1,
    palette='brg',
    col_wrap = 5,
    height=3,
    legend=True,
    facet_kws={'sharey': False, 'sharex': False})
  for axes in g.axes.flat:
      axes.ticklabel_format(axis='both', style='scientific', scilimits=(0, 0))
  plt.gca().set_ylim(bottom=0)
  plt.tight_layout()
  savefig("qp")

plot_qp()

if __name__ == "__main__":
  main()