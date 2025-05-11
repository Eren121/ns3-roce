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

"""
Benchmark unicasts.
Show unreliable or reliable completion time based on the amount of bytes to RDMA Write.

Plot the completion time in `perf.pdf`.
"""

class Model(simulation.Model):
    def __init__(self):
        super().__init__()

        # Add custom inputs.
        self._add_input(simulation.Model.Input("mtu", "MTU."))
        self._add_input(simulation.Model.Input("bytes_to_write", "Amount of bytes to write."))

        # Load default config.
        self._config.update(simulation.load_def_config())

        # Build the topology.
        self.topo = spineleaf.Topology()
        self.topo.n_spines = 3
        self.topo.n_leaf = 4
        self.topo.n_servers_per_leaf = 4
        self._topo.update(self.topo.to_json())
    
    def _configure(self, sim: simulation.Simulation) -> None:
        self._config.update({
            "ns3::RdmaHw::Mtu": self.get("mtu")
        })
        
        self._add_flow({
            "path": "ns3::RdmaFlowUnicast",
            "enable": True,
            "start_time": 0.0,
            "in_background": False,
            "attributes": {
                "SourceNode": 7,
                "DestinationNode": 15,
                "IsReliable": True,
                "PfcPriority": 3,
                "WriteByteAmount": self.get("bytes_to_write")
            }
        })

        self.out_dir = sim.config_dir / ".." / "out"
        self.out_sim_stats_path = self.out_dir / "sim_stats.json"

def main():
    scenarios = simulation.CartesianProduct()
    scenarios.add("mtu", 4096)
    scenarios.add("bytes_to_write", np.linspace(1e6, 100e6))
    
    batch = simulation.Batch()
    batch.run(Model, scenarios)

    rows = []
    for res in batch.res:
        sim = res.sim
        model = res.model
        sim_stats = pyu.load_json(model.out_sim_stats_path)

        row = {}
        row["bytes_to_write"] = model.get("bytes_to_write")
        row["completion_time"] = sim_stats["stop_time"]
        rows.append(row)
    
    df = pd.DataFrame(rows)
    
    sns.set_theme()
    
    sns.scatterplot(data=df, x="bytes_to_write", y="completion_time")
    plt.savefig(batch.img_dir / "perf.pdf")
    plt.clf()



if __name__ == "__main__":
    main()