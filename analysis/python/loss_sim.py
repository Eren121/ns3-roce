#!/usr/bin/env python3
#
# Run the simulation to make the graph X: loss, Y: bandwidth.

import pyutils as pyu
import project
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import itertools
import pathlib
import os
from collections import namedtuple
from dataclasses import dataclass
import tempfile
import json
import topology


def get_path_rel_to_container(path: pathlib.Path) -> str:
    """
    Convert a host path to a container path.
    The path should be mounted in the container.
    Root of the git project is mounted in container.
    """
    if not path.is_relative_to(project.root_path()):
        raise Exception("Path should be inside git directory.")
    return os.path.relpath(path, project.simulation_path()) # Avoid `walk_up` which needs recent Python 3.12

SimulationConfigFile = namedtuple("SimulationConfigFile", "name data")

class SimulationConfig:
    def __init__(self, tmp_dir: pathlib.Path):
        self.templates = {}
        self.tmp_dir = tempfile.TemporaryDirectory(dir=tmp_dir) 
        self.tmp_path = pathlib.Path(self.tmp_dir.name)
        self.custom_data = {}
    
    def set_data(self, key: str, val) -> None:
        self.custom_data[key] = val

    def get_data(self, key: str):
        return self.custom_data[key]

    def add_config_file(self, file: SimulationConfigFile) -> None:
        self.templates[file.name] = file
    
    def generate_config_files(self) -> None:
        for template in self.templates.values():
            template_path = self.tmp_path / template.name
            pyu.write_to_file(template_path, json.dumps(template.data, indent=2))

    def run(self) -> None:
        """
        Working directory of ns-3 process in container is host path `project.simulation_path()`.
        """
        config_name = "config.json"
        if config_name not in self.templates:
            raise Exception(f"Should have at least a {config_name} file in the templates")
        self.generate_config_files()
        config_path = self.tmp_path / config_name
        argv=[
            "make", "-C",
            project.root_path().as_posix(),
            "run",
            f"config={get_path_rel_to_container(config_path)}", "docker_interactive="]
        pyu.run_process(argv=argv)
        return self


def add_missed_chunks_percent_col(res: pd.DataFrame) -> None:
    res[missed_chunks_percent_col] = 0.
    res[bandwidth_col] = 0.
    for i, row in res.iterrows():
        res.at[i, missed_chunks_percent_col] = res.at[i, missed_chunks_col] / total_chunks
        res.at[i, bandwidth_col]             = (total_chunks * chunk_size * 8) / res.at[i, tot_time_col]


def plot(allgather_results: list):
    res = pd.DataFrame.from_records(allgather_results)
    print(res)
    ax = sns.lineplot(data=res, x="bg_bandwidth_percent", y="total_chunk_lost_percent", color=sns.color_palette()[0])
    ax.set_title("Missed chunk % per error rate")
    plt.show()


def main() -> None:
    script_dir = pathlib.Path(__file__).parent
    tmp_dir = script_dir / "out"

    tree_config = topology.FatTreeConfig(depth=2, link_bw=100e9, link_latency=1e-6)
    tree = topology.FatTree(tree_config)

    out_allgather = "out_allgather.json"    
    same_test_count = 1
    configs = []

    for bandwidth_percent in np.repeat(np.linspace(0.01, 0.3, 500), same_test_count):
        flows = {
            "flows": [
                topology.make_flow_allgather(size=1e7, output_file=out_allgather),
                topology.make_flow_multicast(size=1e9, src=4, dst=1, bandwidth_percent=bandwidth_percent)
            ]
        }

        sim_config = json.loads(pyu.read_all_file(script_dir / "default_config.json"))
        config = SimulationConfig(tmp_dir)
        config.add_config_file(SimulationConfigFile(name="config.json", data=sim_config))
        config.add_config_file(SimulationConfigFile(name=sim_config["topology_file"], data=tree.get_topology()))
        config.add_config_file(SimulationConfigFile(name=sim_config["groups_file"], data=tree.get_groups()))
        config.add_config_file(SimulationConfigFile(name=sim_config["trace_file"], data={}))
        config.add_config_file(SimulationConfigFile(name=sim_config["flow_file"], data=flows))
        config.set_data("bg_bandwidth_percent", bandwidth_percent)
        configs.append(config)


    allgather_results = []
    for cur_config in pyu.parallel_for(configs, lambda config: config.run()):
        result = json.loads(pyu.read_all_file(cur_config.tmp_path / out_allgather))
        result["bg_bandwidth_percent"] = cur_config.get_data("bg_bandwidth_percent")
        allgather_results.append(result)

    plot(allgather_results)

if __name__ == "__main__":
    main()