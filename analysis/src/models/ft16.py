import simulation
import numpy as np
import pandas as pd
from typing import List
import pyutils as pyu
from topology import spineleaf
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import matplotlib.ticker as ticker
import pathlib
import json


class Model(simulation.Model):
    """
    2-levels fat tree model to simulate the Allgather.
    A background bisection flow can be added.
    Recovery phase is simulated.
    Multicast phase can be either simulated or pre-filled with a Gilbert-Elliott model for chunk loss.
    """

    def __init__(self):
        super().__init__()
        self._add_input(simulation.Model.Input("per_node", "Per node bytes for the allgather"))
        self._add_input(simulation.Model.Input("root_count", "Allgather parameter", 2))
        self._add_input(simulation.Model.Input("chunk_size", "Allgather parameter"))
        self._add_input(simulation.Model.Input("data", "Count of data chunks per segment", 1))
        self._add_input(simulation.Model.Input("parity", "Count of parity chunks per segment", 0))
        self._add_input(simulation.Model.Input("bisec", "If true, add background bisection traffic", False))
        self._add_input(simulation.Model.Input("ag_stats", "Output file for the allgather stats", "ag-stats.json"))
        self._add_input(simulation.Model.Input("mtu", "MTU", 4096))
        self._config.update(simulation.load_def_config())
        self._config["link_stats_monitor"]["enable"] = True
        self._config["qlen_monitor"]["enable"] = True
    
        self.topo = spineleaf.Topology()
        self.topo.n_leaf = 8
        self.topo.n_spines = 6
        self.topo.n_servers_per_leaf = 4
        self._topo.update(self.topo.to_json())
    
    def _add_bisec(self) -> None:
        first_left = self.topo.first_server()
        first_right = first_left + self.topo.n_servers() // 2
        for i in range(self.topo.n_servers() // 2):
            left = first_left + i
            right = first_right + i
            for j in range(2):
                self._add_flow({
                    "type": "unicast",
                    "background": True,
                    "parameters": {
                        "SrcNode": left if j == 0 else right,
                        "DstNode": right if j == 0 else left,
                        "SrcPort": 900 + 4 * i + 2 * j,
                        "DstPort": 900 + 4 * i + 2 * j + 1,
                        "WritoeSize": 1e9,
                        "BandwidthPercent": 1.0
                    }
                })
                
    def _configure(self, sim: simulation.Simulation) -> None:
        if self.get("bisec"):
            self._add_bisec()

        self._config.update({
            "ns3::RdmaHw::Mtu": self.get("mtu")
        })

        # All the following paths are relative to config.json
        out_dir = pathlib.Path("..") / "out"
        out_recv_chunks = out_dir / "out_recv_chunks.avro"
        self._config["qlen_monitor"]["output"] = (out_dir / "out_qlen.json").as_posix()
        
        self._add_flow({
            "type": "allgather",
            "background": False,
            "parameters": {
                "McastGroup": 1,
                "McastStrategy": "markov",
                "MarkovBurstLength": 50,
                "MarkovGapLength": 500,
                "MarkovGapDensity": 0.001,
                "MarkovBurstDensity": 0.1,
                "PerNodeBytes": self.get("per_node"),
                "RootCount": self.get("root_count"),
                "ChunkSize": self.get("chunk_size"),
                "DataChunkPerSegmentCount": self.get("data"),
                "ParityChunkPerSegmentCount": self.get("parity"),
                "DumpStats": (out_dir / self.get("ag_stats")).as_posix(),
                "DumpMissedChunks": "",
                "DumpRecvChunks": out_recv_chunks.as_posix()
            }
        })

        # All the following paths are absolute
        self.out_dir = sim.config_dir / out_dir
        self.out_recv_chunks = sim.config_dir / out_recv_chunks
        self.out_qlen = sim.config_dir / out_dir / self._config["qlen_monitor"]["output"]


def main():
    max_chunk_size = 5e6
    scenarios = simulation.CartesianProduct()
    scenarios.add("per_node", 1e7)
    scenarios.add("root_count", 2)
    scenarios.add("chunk_size", 4096)
    scenarios.add("mtu", "1500")
    scenarios.add("parity", np.linspace(0, 40, num=10, dtype=int))
    scenarios.add("data", 100)
    scenarios.add("bisec", False)
    
    #scenarios.set_param("chunk_size", np.floor(np.linspace(1, max_chunk_size / mtu, 100)) * mtu)
    #scenarios.set_param("parity", 0)
    
    batch = simulation.Batch()

    batch.run(Model, scenarios)
    plot(batch)

def prepend_keys(prepend: str, d: dict) -> dict:
    return {f"{prepend}{k}": v for k, v in d.items()}


def read_batch_result(batch: simulation.Batch) -> pd.DataFrame:
    rows = []

    # Automatically get the Allgather statistics from the Allgather output stats file to an output variable
    # with the same name prefixed by `ag.`.
    for res in batch.res:
        sim = res.sim
        model = res.model
        ag_stats = pyu.load_json(sim.out_dir / model.get("ag_stats"))
        model_stats = pyu.load_json(sim.config_dir / simulation.Model.model_file)
        row = prepend_keys("ag.", ag_stats) | prepend_keys("model.", model_stats)
        
        # Add total transmitted bytes across all nodes
        tot_tx_bytes = 0
        links_stats = pyu.load_json(sim.config_dir / model._config["link_stats_monitor"]["output_bytes"])
        for link in links_stats:
            tot_tx_bytes += link["bytes"]

        print(f"****** {tot_tx_bytes}")
        row["stats.tot_tx_bytes"] = tot_tx_bytes
        rows.append(row)

    df = pd.DataFrame(rows)
    return df


def recv_to_miss_df(df: pd.DataFrame, total_chunk_count: int, node_count: int) -> pd.DataFrame:
    res_dfs = []

    # Reverse: we want the missed chunks, not the received ones
    # Columns = [node, chunk, time].
    # We want to reverse chunk (remove if present, add if absent).
    for node in range(node_count):
        all_chunks = set(range(total_chunk_count))
        present_chunks = set(df[df.node == node].chunk)
        missing_chunks = sorted(all_chunks - present_chunks)
        missing_df = pd.DataFrame({"chunk": missing_chunks})
        missing_df["node"] = node
        res_dfs.append(missing_df)

        print(df[df.node == node])
        print(missing_df)

    return pd.concat(res_dfs)


def plot_switch_mem(batch: simulation.Batch) -> None:
    for cur_res in batch.res:
        sim, model = cur_res.sim, cur_res.model
        df = pd.DataFrame.from_records(json.load(open(model.out_qlen)))

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

            # Do not save as pdf because there are a lot of data, PDF is too slow to render.
            plt.savefig(sim.img_dir / f"{y}_sw_mem.png")

            # For some reason other plots are not clean (contains red lines etc) if `clf()` not called.
            plt.clf()

def plot_missed_chunks(batch: simulation.Batch) -> None:
    """Plot missed chunks for each simulation of the batch"""
    for cur_res in batch.res:
        sim, model = cur_res.sim, cur_res.model
        ag_stats = pyu.load_json(sim.out_dir / model.get("ag_stats"))
        total_chunk_count = ag_stats["total_chunk_count"]
        node_count = ag_stats["block_count"]
        df = recv_to_miss_df(pyu.read_avro(model.out_recv_chunks), total_chunk_count, node_count)

        sns.relplot(
            df,
            kind='scatter',
            x="chunk", y="node", s=2,
            linewidth=0 # Avoid outline when there is a lot of points
        )

        stats = pyu.load_json(sim.out_dir / model.get("ag_stats"))
        nodes = stats["block_count"]
        chunk_count = stats["total_chunk_count"]
        mtu = model.get("mtu")

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

        # Do not save as pdf because there are a lot of data, PDF is too slow to render.
        plt.savefig(sim.img_dir / "recv_chunks.png")

        # For some reason other plots are not clean (contains red lines etc) if `clf()` not called.
        plt.clf()


def plot(batch: simulation.Batch) -> None:
    plot_switch_mem(batch)
    plot_missed_chunks(batch)
    df = read_batch_result(batch)

    #
    # ag.recovery_elapsed_time:
    #   Total time minus mcast time
    #
    # ag.bandwidth:
    #   Bandwidth (Gb/s) taking into acount only data.
    #
    # ag.cost_ratio:
    #   Relative cost to send data of recovery over the cost to send data over mcast.
    #   Since the recovery part is reliable, it can take more time to send data.
    #   Eg. if `ag.cost_ratio == 1.1`, that means sending data with the recovery is 10% more expensive
    #   than sending data with mcast.
    #   Averaged over all data.

    # Count of blocks in the Allgather
    blocks = df["ag.block_count"]

    # Total count of chunk should be a multiple of the count of blocks.
    assert((df["ag.total_chunk_count"] % df["ag.block_count"] == 0).all())


    # In a single node, this is the total count of data bytes in the Allgather buffer (not taking into account parity bytes).
    # This is taking into account padding, meaning the actual count of data chunks can be different
    # than the requested count of data chunks to make the simulation simpler, by making
    # the count of data chunk perfectly fit in a round count of blocks (and not a partial block at the end).
    df["ag.padded_size"] = df["ag.total_chunk_count"] * df["ag.chunk_size"] / df["ag.block_count"]

    # Total data transmitted during the multicast phase.
    # It is very easy to compute, since each chunk is transmitted once,
    # so the total amount of bytes transmitted is just the count of chunk time the size of a chunk in bytes.
    df["stats.mcast_tx_bytes"] = df["ag.total_chunk_count"] * df["ag.chunk_size"]

    # Total data transmitted during only the recovery phase.
    # Since `tot_tx_bytes` already stores this data (because the multicast phase is only simulated with Markov Chains and not ns3)
    # And we adjust `tot_tx_bytes` in the next line to take into account the mcast phase.
    df["stats.rec_tx_bytes"] = df["stats.tot_tx_bytes"]

    # Total bytes that are sent across all nodes cumulated over all nodes and all the time.
    # Take into account multicast for TX bytes, because we just simulate in markov chains
    df["stats.tot_tx_bytes"] += df["ag.total_chunk_count"] * df["ag.chunk_size"]

    # Remove parity packets
    # assert((df["ag.padded_size"] % (df["model.data"] + df["model.parity"]) == 0).all())
    df["ag.padded_size"] = df["ag.padded_size"] / (df["model.data"] + df["model.parity"]) * df["model.data"]
    print(df["ag.total_data_chunk_count"], '==', df["ag.padded_size"] / df["ag.chunk_size"])
    # assert(df["ag.total_data_chunk_count"] == df["ag.padded_size"] / df["ag.chunk_size"])
    df["ag.recovery_elapsed_time"] = df["ag.total_elapsed_time"] - df["ag.mcast_elapsed_time"]
    df["ag.bandwidth"] = 8 * df["ag.padded_size"] / df["ag.total_elapsed_time"] * df["ag.block_count"] / 1e9 # Gbps

    # Per byte cost (taking into account all data sent, including parity chunks)
    # Approx. : each block does not send its own chunks, so ` - 1`
    mcast_cost = df["ag.mcast_elapsed_time"] / (df["ag.total_chunk_count"] * (blocks - 1))
    recovery_cost = df["ag.recovery_elapsed_time"] / df["ag.lost_data_chunk_count"]

    df["ag.cost_ratio"] = mcast_cost / recovery_cost

    # % of parity chunks added over all chunks
    # eg. 0.1 means 10% of parity chunks are added in addition to the data chunks
    df["model.parity_percent"] = df["model.parity"] / df["model.data"]


    # recovery_cost can be infinite
    df.replace([np.inf, -np.inf], 0.0, inplace=True)

    # Average data loss percentage (taking into account user data only)
    l = df["ag.lost_data_chunk_percent"]

    # Ideal percentage of parity chunks to send
    # if FEC is perfect (any chunk can recover from any chunk)
    df["ideal_parity_percent"] = l / (1 - l)

    
    result_file = batch.batch_dir / "result.txt"
    pyu.write_to_file(result_file, df.to_string())
    
    toplot = []
    x = ["model.parity_percent", "normalized"]
    #x = ["parity", "packets"]
    toplot.append(simulation.PlotData(*x, "ag.lost_chunk_percent", "normalized"))
    toplot.append(simulation.PlotData(*x, ["ag.recovery_elapsed_time", "ag.mcast_elapsed_time"], "second", "time"))
    toplot.append(simulation.PlotData(*x, "ag.bandwidth", "Gbps"))
    toplot.append(simulation.PlotData(*x, "ag.padded_size", "B"))
    toplot.append(simulation.PlotData(*x, ["ag.lost_data_chunk_percent", "ideal_parity_percent"], "normalized", "axis"))
    toplot.append(simulation.PlotData(*x, "ag.cost_ratio", "mcast / recovery"))
    toplot.append(simulation.PlotData(*x, "stats.tot_tx_bytes", "bytes"))
    toplot.append(simulation.PlotData(*x, "stats.mcast_tx_bytes", "bytes"))
    toplot.append(simulation.PlotData(*x, "stats.rec_tx_bytes", "bytes"))
    toplot.append(simulation.PlotData(*x, "ag.retr_chunks_tot", "chunks"))
    simulation.plot(df, toplot, batch.img_dir)    

if __name__ == "__main__":
    main()