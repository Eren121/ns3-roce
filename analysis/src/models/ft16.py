import simulation
import numpy as np
import pandas as pd
from typing import List
import pyutils as pyu


class Model(simulation.Model):
    """
    Fat tree model wit 16 leaf nodes.
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
        self._config.update(simulation.load_def_config())
        self._topo.update(simulation.load_topo("ft16"))
    
    def _add_bisec(self) -> None:
        first_left = 6
        first_right = 14
        for i in range(8):
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
                        "WriteSize": 1e9,
                        "BandwidthPercent": 1.0
                    }
                })
                
    def _configure(self) -> None:
        if self.get("bisec"):
            self._add_bisec()
        
        out_dir = "../out/" # Relative to config.json
        self._add_flow({
            "type": "allgather",
            "background": False,
            "parameters": {
                "McastGroup": 1,
                "McastStrategy": "markov",
                "MarkovBurstLength": 100,
                "MarkovGapLength": 50,
                "PerNodeBytes": self.get("per_node"),
                "RootCount": self.get("root_count"),
                "ChunkSize": self.get("chunk_size"),
                "DataChunkPerSegmentCount": self.get("data"),
                "ParityChunkPerSegmentCount": self.get("parity"),
                "DumpStats": out_dir + self.get("ag_stats"),
                "DumpMissedChunks": ""
            }
        })


def main():
    max_chunk_size = 5e6
    scenarios = simulation.CartesianProduct()
    scenarios.add("per_node", 1e8)
    scenarios.add("root_count", 2)
    scenarios.add("chunk_size", 9000)
    scenarios.add("parity", np.arange(10))
    scenarios.add("data", 10)
    scenarios.add("bisec", False)
    
    #scenarios.set_param("chunk_size", np.floor(np.linspace(1, max_chunk_size / mtu, 100)) * mtu)
    #scenarios.set_param("parity", 0)
    
    batch = simulation.Batch()
    batch.run(Model, scenarios)
    plot(batch)


def prepend_keys(prepend: str, d: dict) -> dict:
    return {f"{prepend}{k}": v for k, v in d.items()}


def plot(batch: simulation.Batch) -> None:
    rows = []
    for res in batch.res:
        sim = res.sim
        model = res.model
        ag_stats = pyu.load_json(sim.out_dir / model.get("ag_stats"))
        model_stats = pyu.load_json(sim.config_dir / simulation.Model.model_file)
        row = prepend_keys("ag.", ag_stats) | prepend_keys("model.", model_stats)
        rows.append(row)
    df = pd.DataFrame(rows)

    # padded_size: per node data in bytes
    # Size taking into account padding (due to ceil division rounding of packets/chunks/segments)
    assert((df["ag.total_chunk_count"] % df["ag.block_count"] == 0).all())
    df["ag.padded_size"] = df["ag.total_chunk_count"] * df["ag.chunk_size"] / df["ag.block_count"]

    # Remove parity packets
    assert((df["ag.padded_size"] % (df["model.data"] + df["model.parity"]) == 0).all())
    df["ag.padded_size"] = df["ag.padded_size"] / (df["model.data"] + df["model.parity"]) * df["model.data"]

    df["ag.recovery_elapsed_time"] = df["ag.total_elapsed_time"] - df["ag.mcast_elapsed_time"]
    df["ag.bandwidth"] = 8 * df["ag.padded_size"] / df["ag.total_elapsed_time"] * df["ag.block_count"] / 1e9 # Gbps
    
    result_file = batch.batch_dir / "result.txt"
    pyu.write_to_file(result_file, df.to_string())

    toplot = []
    x = ["model.parity", "chunks"]
    #x = ["parity", "packets"]
    toplot.append(simulation.PlotData(*x, "ag.lost_chunk_percent", "normalized"))
    toplot.append(simulation.PlotData(*x, ["ag.recovery_elapsed_time", "ag.mcast_elapsed_time"], "second", "time"))
    toplot.append(simulation.PlotData(*x, "ag.bandwidth", "Gbps"))
    toplot.append(simulation.PlotData(*x, "ag.padded_size", "B"))
    simulation.plot(df, toplot, batch.img_dir)
    

if __name__ == "__main__":
    main()