import simulation
import numpy as np
import pandas as pd
from typing import List
import pyutils as pyu
from topology import spineleaf
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator, MultipleLocator
import matplotlib.ticker as ticker
import pathlib
import json

"""
Plot the difference by having a random loss bitmap vs 
being how it is, that is with correlation of loss between ranks `n` and `n+1`.
"""

class Model(simulation.Model):
    def __init__(self):
        super().__init__()

        # Register inputs.
        self.background_bisection_flow_input = simulation.Input("background_bisection_flow", "Adds a background bisection flow.", False)
        self._add_input(self.background_bisection_flow_input)
        
        self.per_node_chunk_count_input = simulation.Input("per_node_chunk_count", "Allgather parameter.", 256)
        self._add_input(self.per_node_chunk_count_input)
        
        # Register output files.
        self.bitmaps_avro_file = "ag_recv_chunks.avro"
        self.stats_json_file = "ag_stats.json"
        
    def _configure(self) -> None:
        # Use spineleaf topology.
        simulation.set_spineleaf_topology(self, 8, 16, 4)

        # Add background bisection flow.
        if self.background_bisection_flow_input.current_value:
            self._flows.append(simulation.make_bisection_flow(bytes=1e9, in_background=True))

        self._flows.append(simulation.make_allgather_flow(
            self,
            root_count=2,
            per_node_chunk_count=self.per_node_chunk_count_input.current_value,
            per_chunk_pkt_count=16, # To have 64KiB per chunk.
            optimize_throughput=True,
            bitmaps_avro_file=self.bitmaps_avro_file,
            stats_json_file=self.stats_json_file,
        ))


def main():
    scenarios = simulation.CartesianProduct()
    scenarios.add("background_bisection_flow", False)
    scenarios.add("per_node_chunk_count", 256)

    batch = simulation.Batch(Model, scenarios)
    simulations = batch.run()

    for sim in simulations:
        ag_stats = pyu.load_json(sim.get_output_path(sim.model.stats_json_file))
        bitmaps = pyu.read_avro(sim.get_output_path(sim.model.bitmaps_avro_file))
        num_ranks = ag_stats["num_ranks"]
        num_pkts_per_chunk = ag_stats["num_pkts_per_chunk"]
        num_chunks_per_rank = ag_stats["num_chunks_per_rank"]
        num_pkts_per_mcast = num_pkts_per_chunk * num_chunks_per_rank
        ranks = np.arange(num_ranks)
        pkts = np.arange(num_pkts_per_mcast * num_ranks)
        bitmaps_perfect = pd.DataFrame({"dst_rank": ranks})
        bitmaps_perfect = bitmaps_perfect.merge(pd.DataFrame({"packet": pkts}), how="cross")
        misses = len(bitmaps_perfect) - len(bitmaps)
        loss_ratio = misses / len(bitmaps_perfect)
        bitmaps_random = bitmaps_perfect.sample(frac=(1. - loss_ratio))
        
        print(f"Loss ratio: {loss_ratio}")
        plot(ag_stats, bitmaps, simulation.mkdir_p(batch.img_dir / "simulation"))
        plot(ag_stats, bitmaps_random, simulation.mkdir_p(batch.img_dir / "random"))


def plot(ag_stats: pd.DataFrame, bitmaps: pd.DataFrame, img_dir: pathlib.Path) -> None:
    # This DataFrame `bitmaps_perfect` contains all the bitmaps if there is no miss.
    num_ranks = ag_stats["num_ranks"]
    num_pkts_per_chunk = ag_stats["num_pkts_per_chunk"]
    num_chunks_per_rank = ag_stats["num_chunks_per_rank"]
    num_pkts_per_mcast = num_pkts_per_chunk * num_chunks_per_rank

    ranks = np.arange(num_ranks)
    pkts = np.arange(num_pkts_per_mcast * num_ranks)
    
    bitmaps_perfect = pd.DataFrame({"dst_rank": ranks})
    bitmaps_perfect = bitmaps_perfect.merge(pd.DataFrame({"packet": pkts}), how="cross")

    bitmaps.drop("time", axis="columns", inplace=True, errors="ignore")
    bitmaps.drop("src_rank", axis="columns", inplace=True, errors="ignore")
    bitmaps = bitmaps.sort_values(by=["dst_rank", "packet"]).reset_index(drop=True)

    x = bitmaps_perfect.merge(
        bitmaps.drop_duplicates(),
        on=['dst_rank', 'packet'], how="left", indicator=True)
    bitmaps_misses = (x[x["_merge"] == "left_only"]).reset_index(drop=True)
    bitmaps_misses.drop("_merge", axis="columns", inplace=True)

    print(bitmaps_perfect)
    print("Perfect bitmap size: ", len(bitmaps_perfect))
    print("Simulation bitmap size: ", len(bitmaps))
    print("Misses: ", len(bitmaps_perfect) - len(bitmaps))

    bitmaps_misses["src_rank"] = bitmaps_misses["packet"] / num_pkts_per_mcast

    sns.set_theme()
    sns.scatterplot(data=bitmaps_misses, x="src_rank", y="dst_rank",
                    s=10, alpha=0.05, edgecolor=None, linewidth=0)
    ax = plt.gca()
    ax.set_xlim(-0.5, num_ranks + 0.5)
    ax.set_ylim(-0.5, num_ranks - 0.5)
    ax.xaxis.set_major_locator(MultipleLocator(1))
    ax.yaxis.set_major_locator(MultipleLocator(1))
    plt.title("Missed chunks")
    plt.savefig(img_dir / "missed-chunks.png")
    plt.clf()


    # Check miss chain repartition:
    # 0: Packets missed.
    # 1: Packets missed and by the direct neighbor.
    # 2: Packets missed and by the direct neighbor and its neighbor.
    # etc..
    miss_chains = np.zeros(num_ranks)

    for first_rank in range(num_ranks):
        first_rank_misses = bitmaps_misses[bitmaps_misses["dst_rank"] == first_rank]
        
        # Left join for each neighbor, recursively.
        for i in range(num_ranks):
            second_rank = (first_rank + i) % num_ranks
            second_rank_misses = bitmaps_misses[bitmaps_misses["dst_rank"] == second_rank].copy()
            second_rank_misses.drop("src_rank", axis="columns", inplace=True, errors="ignore")
            second_rank_misses.drop("dst_rank", axis="columns", inplace=True, errors="ignore")
            x = first_rank_misses.merge(
                second_rank_misses.drop_duplicates(),
                on=['packet'], how="left", indicator=True)
            first_rank_misses = (x[x["_merge"] == "both"]).reset_index(drop=True)
            first_rank_misses.drop("_merge", axis="columns", inplace=True)

            miss_chains[i] += len(first_rank_misses)
    
    # Since each chain is counted multiple time, correct the values.
    print(miss_chains)
    for i in range(len(miss_chains) - 1, 0, -1):
        for j in range(i):
            miss_chains[j] -= miss_chains[i] * (i - j + 1)
    miss_chains = miss_chains.astype(int)
    print(miss_chains)
    s = 0
    for i in range(len(miss_chains)):
        s += (i + 1) * miss_chains[i]
    print(s)

    miss_chains = pd.DataFrame({
        "length": list(range(len(miss_chains))),
        "count": miss_chains
    })

    sns.scatterplot(data=miss_chains, x="length", y="count")
    ax = plt.gca()
    ax.set_xlim(-0.5, num_ranks + 0.5)
    ax.xaxis.set_major_locator(MultipleLocator(1))
    ax.set_yscale("log")
    plt.title("Adjacent missed chunks")
    plt.savefig(img_dir / "adjacent-missed-chunks.pdf")
    plt.savefig(img_dir / "adjacent-missed-chunks.png")
    plt.clf()

if __name__ == "__main__":
    main()