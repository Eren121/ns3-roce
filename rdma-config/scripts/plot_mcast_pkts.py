#!/usr/bin/env python3

import pandas as pd
import json
import pathlib
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import matplotlib.ticker as ticker
from avro.io import DatumReader, DatumWriter
from avro.datafile import DataFileReader, DataFileWriter

def print_stats(df):
    stats = json.load(open("../out/mcast_pkts.json"))
    
    print(df.head())
    print("Packets successfully received: ", len(df))

    ranks = stats["num_ranks"]
    chunks_per_rank = stats["num_chunks_per_rank"]
    pkts_per_chunk = stats["num_pkts_per_chunk"]
    print("Total packet count: ", pkts_per_chunk * chunks_per_rank * ranks ** 2)


def main():
    sns.set_theme()
    sns.set_palette("tab20")

    df = read_avro("../out/mcast_pkts.avro")
    print_stats(df)

    # We plot a lot of points, lines is good
    g = sns.FacetGrid(df, col="dst_rank", col_wrap=4, hue="src_rank")
    g.map(sns.lineplot, "time", "packet")
    g.add_legend()

    plt.savefig("../out/mcast_pkts.png")
    plt.clf()


def read_avro(path) -> pd.DataFrame:
    with open(path, "rb") as file:
        reader = DataFileReader(file, DatumReader())
        return pd.DataFrame.from_records([r for r in reader])


if __name__ == "__main__":
    main()