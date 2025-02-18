import pandas as pd
import json
import pathlib
import seaborn as sns
import matplotlib.pyplot as plt

script_dir = pathlib.Path(__file__).parent
df = pd.DataFrame.from_records(json.load(open(script_dir / "out_qlen.txt")))
print(df[df.bytes > 0])

sns.set_theme()

sns.relplot(
  df,
  kind='line',
  x="time", y="bytes", hue="iface",
  row="node",
  palette='plasma', aspect=4, height=2.5, alpha=0.7, style='iface',
  facet_kws={'sharey': False, 'sharex': False})

plt.show()