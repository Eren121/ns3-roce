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
import uuid
from rich.progress import Progress

script_dir = pathlib.Path(__file__).parent
tmp_dir = script_dir / "out"

def get_path_rel_to_container(path: pathlib.Path) -> str:
    """
    Convert a host path to a container path.
    The path should be mounted in the container.
    Root of the git project is mounted in container.
    """
    if not path.is_relative_to(project.root_path()):
        raise Exception("Path should be inside git directory.")
    return os.path.relpath(path, project.root_path())


SimulationConfigFile = namedtuple("SimulationConfigFile", "name data")


class CartesianProduct:
    def __init__(self):
        self.vars = {}
    
    def set_param(self, name: str, values: list) -> None:
        self.vars[name] = values

    def values(self):
        """Iterate the cartesian product of all values."""
        var_values = []
        var_names = []
        for name, values in self.vars.items():
            var_values.append(values)
            var_names.append(name)

        var_indices = []
        for values in var_values:
            var_indices.append(np.arange(0, len(values)))
        
        ret = []
        for indices in itertools.product(*var_indices):
            cur = {}
            for idx, off in enumerate(indices):
                cur[var_names[idx]] = var_values[idx][off]
            ret.append(cur)

        return ret;

class SimulationConfig:
    def __init__(self, tmp_dir: pathlib.Path, keep_dir: bool = False):
        self.templates = {}
        self.custom_data = {}

        tmp_dir.mkdir(parents=True, exist_ok=True)

        if keep_dir:
            self.tmp_path = tmp_dir / str(uuid.uuid4())
            self.tmp_path.mkdir(parents=True, exist_ok=True)
        else:
            self.tmp_dir = tempfile.TemporaryDirectory(dir=tmp_dir)
            self.tmp_path = pathlib.Path(self.tmp_dir.name)
            
        self.stdout_path = self.tmp_path / "stdout.txt"
        self.stdout_file = open(self.stdout_path, "w")

    
    def set_data(self, key: str, val) -> None:
        self.custom_data[key] = val

    def get_data(self, key: str):
        return self.custom_data[key]

    def get_all_data(self) -> dict:
        return self.custom_data

    def add_config_file(self, file: SimulationConfigFile) -> None:
        self.templates[file.name] = file
    
    def generate_config_files(self) -> None:
        for template in self.templates.values():
            template_path = self.tmp_path / template.name
            pyu.write_to_file(template_path, json.dumps(template.data, indent=2))

    def on_each_line(self, line: str) -> None:
        self.stdout_file.write(line)
        
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
            f"app_config={get_path_rel_to_container(config_path)}",
            "docker_interactive="]
        pyu.run_process(argv=argv, each_line=self.on_each_line)
        self.stdout_file.close()
        return self


def plot(res: pd.DataFrame) -> None:
    sns.set_theme()
    img_dir = tmp_dir / "img"
    img_dir.mkdir(parents=True, exist_ok=True)

    for y in ["elapsed_recovery_time", "total_chunk_lost_percent", "elapsed_mcast_time", "elapsed_total_time"]:
        x = "bg_bandwidth_percent"
        ax = sns.lineplot(data=res, x=x, y=y, color=sns.color_palette()[0])
        ax.set_title(f"{y}")
        fig = ax.get_figure()
        fig.tight_layout()
        fig.savefig(img_dir / f"{y}.pdf")
        fig.clf()

def run_cases() -> list:
    tree_config = topology.FatTreeConfig(depth=2, link_bw=100e9, link_latency=1e-6)
    tree = topology.FatTree(tree_config)
    out_allgather = "out_allgather.json"

    # flip to start with longer cases, to have time upper bound estimation
    params_cases = CartesianProduct()
    params_cases.set_param("bg_bandwidth_percent", np.flip(np.linspace(0.0, 0.8, 60)))
    params_cases.set_param("iteration", np.arange(2))
    
    enable_anim = False
    keep_dir = True
    
    def generate_config():
        for params in params_cases.values():
            flows = {
                "flows": [
                    topology.make_flow_allgather(size=1e7, output_file=out_allgather),
                    topology.make_flow_multicast(size=1e9, src=4, dst=1, bandwidth_percent=params["bg_bandwidth_percent"])
                ]
            }

            sim_config = json.loads(pyu.read_all_file(script_dir / "default_config.json"))

            config = SimulationConfig(tmp_dir / "sim", keep_dir=keep_dir)
            config.add_config_file(SimulationConfigFile(name="config.json", data=sim_config))
            config.add_config_file(SimulationConfigFile(name=sim_config["topology_file"], data=tree.get_topology()))
            config.add_config_file(SimulationConfigFile(name=sim_config["groups_file"], data=tree.get_groups()))
            config.add_config_file(SimulationConfigFile(name=sim_config["trace_file"], data={}))
            config.add_config_file(SimulationConfigFile(name=sim_config["flow_file"], data=flows))
            sim_config["anim_output_file"] = "x.xml" if enable_anim else ""
            
            for name, value in params.items():
                config.set_data(name, value)
            
            yield config

    progress = Progress()

    with progress:
        progress_task = progress.add_task("running all simulation cases...", total=len(params_cases.values()))

        def run_config(config):
            config.run()
            progress.update(progress_task, advance=1)
            return config

        allgather_results = []
        for cur_config in pyu.parallel_for(data=generate_config(), func=run_config):
            result = json.loads(pyu.read_all_file(cur_config.tmp_path / out_allgather))
            result.update(cur_config.get_all_data())
            allgather_results.append(result)

    return allgather_results

def main() -> None:
    results_json_path = tmp_dir / "results.json"
    results_txt_path = tmp_dir / "results.txt"

    if results_json_path.exists():
        print("Results exist, skip simulation")
        res = pd.DataFrame.from_records(json.loads(pyu.read_all_file(results_json_path)))
    else:
        res = pd.DataFrame.from_records(run_cases())
        pyu.write_to_file(results_json_path, res.to_json(orient="records", indent=4))
 
    print(res)
    res["elapsed_recovery_time"] = res["elapsed_total_time"] - res["elapsed_mcast_time"]
    pyu.write_to_file(results_txt_path, res.to_string())    
    plot(res)

if __name__ == "__main__":
    main()