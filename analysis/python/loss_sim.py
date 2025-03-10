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
import shutil
from rich.progress import Progress, MofNCompleteColumn 

#############################
## Fixed global parameters

script_dir = pathlib.Path(__file__).parent
out_allgather = "out_allgather.json"
tmp_dir = script_dir / "out"
sim_dir = tmp_dir / "sim"
mtu = 1500

#############################
## Modifiable global parameters
    
enable_anim = False
keep_dir = False
overwrite_results = True

#############################

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
    
    def set_param(self, name: str, values) -> None:
        self.vars[name] = np.atleast_1d(values)

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
            self.tmp_path = pathlib.Path(tempfile.mkdtemp(suffix=None, prefix=None, dir=tmp_dir))
        else:
            self.tmp_dir = tempfile.TemporaryDirectory(dir=tmp_dir)
            self.tmp_path = pathlib.Path(self.tmp_dir.name)
            
        self.stdout_path = self.tmp_path / "stdout.txt"
        self.stdout_file = open(self.stdout_path, "w")

    
    def set_data(self, key: str, val) -> None:
        self.custom_data[key] = val

    def get_data(self, key: str):
        return self.custom_data[key]

    def insert_fields(self, result: dict) -> None:
        result.update(self.custom_data)
        result["tmp_dir"] = str(self.tmp_path)

    def add_config_file(self, file: SimulationConfigFile) -> None:
        self.templates[file.name] = file
    
    def generate_config_files(self) -> None:
        for template in self.templates.values():
            template_path = self.tmp_path / template.name
            pyu.write_to_file(template_path, pyu.dump_json(template.data))

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


class PlotData:
    def __init__(self, x, x_unit, ys, y_unit, type=None):
        self.x = x
        self.x_unit = x_unit
        self.ys = np.atleast_1d(ys)
        self.y_unit = y_unit
        self.type = type # When len(ys) > 1, describe the type of each y
        
        if self.type is None:
            assert(len(self.ys) == 1)
            self.type = self.ys[0]

    
def plot(res: pd.DataFrame) -> None:
    sns.set_theme()
    img_dir = tmp_dir / "img"
    img_dir.mkdir(parents=True, exist_ok=True)

    toplot = []
    x = ["parity", "B"]
    #x = ["parity", "packets"]
    toplot.append(PlotData(*x, "lost_chunk_percent", "percent"))
    toplot.append(PlotData(*x, ["recovery_elapsed_time", "mcast_elapsed_time"], "second", "time"))
    toplot.append(PlotData(*x, "bandwidth", "Gbps"))
    toplot.append(PlotData(*x, "padded_size", "B"))
    
    for data in toplot:
        melted = res.melt(
            id_vars=data.x,
            value_vars=data.ys,
            var_name=data.type,
            value_name=data.y_unit)
        
        print(melted)
        
    
        ymax = melted[data.y_unit].max() * 1.3
        
        ax = sns.lineplot(melted, x=data.x, y=data.y_unit, hue=data.type)
        ax.set_title(f"{data.type}")
        ax.set_ylim(bottom=0, top=ymax)
        ax.set_xlabel(f"{data.x} ({data.x_unit})")
        ax.set_ylabel(f"{data.type} ({data.y_unit})")
        fig = plt.gcf()
        fig.tight_layout()
        fig.savefig(img_dir / f"{data.type}-{data.x}.pdf")
        fig.clf()



def model1(params_cases):
    """
    Fat tree model wit 16 leaf nodes.
    Bisection background traffic.
    Parameters:
        size: Per node size for the allgather.
        root_count: Allgather parameter.
        chunk_size: Allgather parameter.
        data: Count of data chunks per segment.
        parity: Count of parity chunks per segment.
        bisec: If true, add background bisection traffic.
    """
    for params in params_cases.values():
        flows = []
        if params["bisec"]:
            first_left = 6
            first_right = 14
            for i in range(8):
                left = first_left + i
                right = first_right + i
                for j in range(2):
                    flows.append({
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
        flows.append({
            "type": "allgather",
            "background": False,
            "parameters": {
                "McastGroup": 1,
                "McastStrategy": "markov",
                "PerNodeBytes": params["size"],
                "RootCount": params["root_count"],
                "ChunkSize": params["chunk_size"],
                "DataChunkPerSegmentCount": 20,
                "ParityChunkPerSegmentCount": params["parity"],
                "DumpStats": out_allgather,
                "DumpMissedChunks": ""
            }
        })

        config_dir = script_dir / "config"
        fat_tree_depth2_dir = config_dir / "fat-tree-depth2"
        
        sim_config = pyu.load_json(config_dir / "default_config.json")
        
        config = SimulationConfig(sim_dir, keep_dir=keep_dir)
        config.add_config_file(SimulationConfigFile(name="config.json", data=sim_config))
        config.add_config_file(SimulationConfigFile(name=sim_config["topology_file"], data=pyu.load_json(fat_tree_depth2_dir / "topology.json")))
        config.add_config_file(SimulationConfigFile(name=sim_config["trace_file"], data={}))
        config.add_config_file(SimulationConfigFile(name=sim_config["flow_file"], data={"flows": flows}))
        sim_config["anim_output_file"] = "anim.xml" if enable_anim else ""
        
        for name, value in params.items():
            config.set_data(name, value)
        
        yield config


def run_cases(model, params_cases) -> list:
    config_generator = model(params_cases)

    # Clear the simulation output folder
    if sim_dir.exists():
        shutil.rmtree(sim_dir)
        sim_dir.mkdir(parents=True, exist_ok=True)
    
    progress = Progress(
        *Progress.get_default_columns(),
        MofNCompleteColumn()
    )

    with progress:
        progress_task = progress.add_task("running all simulation cases...", total=len(params_cases.values()))

        def run_config(config):
            config.run()
            progress.update(progress_task, advance=1)
            return config

        allgather_results = []
        for cur_config in pyu.parallel_for(data=config_generator, func=run_config):
            result = pyu.load_json(cur_config.tmp_path / out_allgather)
            cur_config.insert_fields(result)
            allgather_results.append(result)
    
    return allgather_results

def ensure_built() -> None:
    argv=[
        "make", "-C",
        project.root_path().as_posix(),
        "build", "docker_interactive="]
    pyu.run_process(argv=argv)
    

def main() -> None:
    ensure_built()
    results_json_path = tmp_dir / "results.json"
    results_txt_path = tmp_dir / "results.csv"

    # `np.flip()` to start with longer cases, to have time upper bound estimation


    max_chunk_size = 5e6
    params_cases = CartesianProduct()
    params_cases.set_param("iteration", np.arange(1))
    params_cases.set_param("size", 1e7)
    params_cases.set_param("root_count", 2)
    #params_cases.set_param("chunk_size", np.floor(np.linspace(1, max_chunk_size / mtu, 100)) * mtu)
    params_cases.set_param("chunk_size", 9000)
    #params_cases.set_param("parity", 0)
    params_cases.set_param("parity", np.arange(10))
    params_cases.set_param("data", 10)
    params_cases.set_param("bisec", False)

    if results_json_path.exists() and not overwrite_results:
        print("Results exist, skip simulation")
        res = pd.DataFrame.from_records(json.loads(pyu.read_all_file(results_json_path)))
    else:
        res = pd.DataFrame.from_records(run_cases(model1, params_cases))
        pyu.write_to_file(results_json_path, res.to_json(orient="records", indent=4))
        
    # Size taking into account padding (due to ceil division rounding of packets/chunks/segments)
    res["padded_size"] = res["total_chunk_count"] * res["chunk_size"] / res["block_count"]
    # Remove parity packets
    res["padded_size"] = res["padded_size"] / (res["data"] + res["parity"]) * res["data"]

    res["recovery_elapsed_time"] = res["total_elapsed_time"] - res["mcast_elapsed_time"]
    res["bandwidth"] = 8 * res["padded_size"] / res["total_elapsed_time"] * res["block_count"] / 1e9 # Gbps
    
    pyu.write_to_file(results_txt_path, res.to_string())
    plot(res)

if __name__ == "__main__":
    main()