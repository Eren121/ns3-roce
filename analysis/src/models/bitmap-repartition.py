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
Plot the difference by having a random loss bitmap vs 
being how it is, that is with correlation of loss between ranks `n` and `n+1`.
"""

class Model(simulation.Model):
    def __init__(self):
        super().__init__()

        # Register inputs.
        self.background_bisection_flow = simulation.Input("background_bisection_flow", "Adds a background bisection flow.", False)
        self._add_input(self.background_bisection_flow)
        
        self.per_node_chunk_count = simulation.Input("per_node_chunk_count", "Allgather parameter.", 256)
        self._add_input(self.per_node_chunk_count)
        
        # Register output files.
        self.bitmaps_avro_file = "ag_recv_chunks.avro"
        self.stats_json_file = "ag_stats.json"
        
    def _configure(self, sim: simulation.Simulation) -> None:
        # Use spineleaf topology.
        simulation.set_spineleaf_topology(self, 2, 4, 4)

        # Add background bisection flow.
        if self.background_bisection_flow.current_value:
            self._flows.append(simulation.make_bisection_flow(bytes=1e9, in_background=True))

        self._flows.append(simulation.make_allgather_flow(
            self,
            root_count=2,
            per_node_chunk_count=self.per_node_chunk_count,
            per_chunk_pkt_count=16, # To have 64KiB per chunk.
            optimize_throughput=True,
            bitmaps_avro_file=self.bitmaps_avro_file,
            stats_json_file=self.stats_json_file,
        ))

def main():
    scenarios = simulation.CartesianProduct()
    scenarios.add("background_bisection_flow", False)
    scenarios.add("per_node_chunk_count", 32)

    batch = simulation.Batch(Model, scenarios)
    batch.run()
    
if __name__ == "__main__":
    main()