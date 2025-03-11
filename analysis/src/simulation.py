import pathlib
import itertools
import numpy as np
import tempfile
from datetime import datetime
from collections import namedtuple
from rich.progress import Progress, MofNCompleteColumn 
from typing import List
import project
import pyutils as pyu
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd


def ensure_built() -> None:
    argv=[
        "make", "-C",
        project.root_path().as_posix(),
        "build", "docker_interactive="]
    pyu.run_process(argv=argv)


def mkdir_p(path: pathlib.Path):
    path.mkdir(parents=True)
    return path


def make_timestamp_dir(parent: pathlib.Path):
    """
    Generate a directory based on timestamp.
    In the case the directory already exist,
    if two runs in the same seconds, then wait 1 second
    so that the name is unique.
    """
    while True:
        format = "%Y%m%d-%Hh%M-%S.%f"
        name = datetime.now().strftime(format)
        path = parent / name
        if not path.exists():
            break
    return mkdir_p(path)


def load_def_config() -> dict:
    config_dir = project.analysis_path() / "config"
    return pyu.load_json(config_dir / "default_config.json")

def load_topo(name: str) -> dict:
    config_dir = project.analysis_path() / "config"
    topo_dir = config_dir / "topologies"
    return pyu.load_json(topo_dir / f"{name}.json")
    
class CartesianProduct:
    """
    Permits to run the cartesian product of all the parameters.
    ```
    c = CartesianProduct()
    c.add("x", [1, 2, 3])
    c.add("y", [10, 20, 30])
    for p in c:
        print(p["x"])
        print(p["y"])
    ```
    Will iterate the cartesian product of `x` and `y`.
    """

    def __init__(self):
        self.vars = {}
    
    def add(self, name: str, values) -> None:
        self.vars[name] = np.atleast_1d(values)
    
    def __len__(self):
        if len(self.vars) == 0:
            return 0
        tot = 1
        for array in self.vars.values():
            tot *= len(array)
        return tot 

    def __iter__(self):
        """
        Iterate the cartesian product of all values
        as a dictionary.
        """
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

        return iter(ret)


"""
The simulation needs configuration file next
to the main configuration file.
"""
SimulationConfigFile = namedtuple(
    "SimulationConfigFile", "name data")


class Simulation:
    def __init__(self, sim_dir: pathlib.Path, keep_dir: bool):
        self.templates = {}
        self.sim_dir = mkdir_p(sim_dir)
        self.config_dir = mkdir_p(self.sim_dir / "config")
        self.out_dir = mkdir_p(self.sim_dir / "out")
        self.stdout_path = self.out_dir / "stdout.txt"
        self.stdout_file = open(self.stdout_path, "w")
    
    def add_config(self, file: SimulationConfigFile) -> None:
        self.templates[file.name] = file
    
    def _generate_config_files(self) -> None:
        for template in self.templates.values():
            template_path = self.config_dir / template.name
            pyu.write_to_file(template_path, pyu.dump_json(template.data))

    def _on_each_line(self, line: str) -> None:
        self.stdout_file.write(line)
    
    def run(self) -> None:
        """
        Working directory of ns-3 process in container is host path `project.simulation_path()`.
        """
        main_config = "config.json"
        if main_config not in self.templates:
            raise Exception(f"Should have at least a '{main_config}' file in the config files")
        self._generate_config_files()
        config_path = self.config_dir / main_config
        argv=[
            "make", "-C",
            project.root_path().as_posix(),
            "run",
            f"app_config={project.get_path_rel_to_container(config_path)}",
            "docker_interactive="]
        pyu.run_process(argv=argv, each_line=self._on_each_line)
        self.stdout_file.close()
        return self


class Model:
    """
    Inherit this class to define your own model.
    
    A model runs the simulation based on
    user-defined high-level input parameters.

    Call `_add_input()` for each input in the constructor,
    and modify the configuration accordingly by overriding
    `_configure()`.
    Users set inputs by calling `set()`.
    """

    # Will write all the parameters of the model
    # in this file located in the config.json dir
    model_file = "model.json"

    class Input:
        def __init__(self, name, desc, def_val=None):
            self.name = name
            self.desc = desc
            self.def_val = def_val

    def __init__(self):
        self._flows = []
        self._topo = {}
        self._config = {}
        self.__inputs_info = {}
        self.__inputs = {}
    
    def _add_flow(self, flow):
        self._flows.append(flow)

    def _add_input(self, input: Input) -> None:
        self.__inputs_info[input.name] = input
        self.set(input.name, input.def_val)
    
    def set(self, input: str, val) -> None:
        if input not in self.__inputs_info:
            raise Exception(f"Input '{input}' does not exist in model")
        self.__inputs[input] = val
    
    def get(self, input: str):
        return self.__inputs[input]

    def inputs(self):
        """
        Iterate all input values of the current instance of the model.
        """
        return self.__inputs

    def _configure(self) -> None:
        """Override in child"""
        raise NotImplementedError()

    def configure(self, sim: Simulation) -> None:
        self._configure()
        sim.add_config(SimulationConfigFile("config.json", self._config))
        sim.add_config(SimulationConfigFile("topology.json", self._topo))
        sim.add_config(SimulationConfigFile("flows.json", {"flows": self._flows}))

        # Not used by C++, but to keep as informative
        sim.add_config(SimulationConfigFile(Model.model_file, self.__inputs))


class BatchResult:
    def __init__(self, sim: Simulation, model):
        self.sim = sim
        self.model = model

    
class Batch:
    """
    Run multiple time simulations by varying the desired parameters
    via a cartesian product.
    """
    
    def __init__(self):
        # Directory where to store all files generated by the simulation
        # (graphs, data, and simulation output)
        self.batch_dir = make_timestamp_dir(project.out_path())
        
        # Each simulation runs in a dedicated dir. This is the parent of all of them.
        self.runs_dir = mkdir_p(self.batch_dir / "runs")
        
        # Store the aggregated plots
        self.img_dir = mkdir_p(self.batch_dir / "img")
        
        # Will store all the results
        self.res = None

    def run(self, model_class, scenarios: CartesianProduct) -> List[BatchResult]:
        """
        Run the model for each value of the cartesian product.
        """

        all_cases = []
        ensure_built()
        progress = Progress(*Progress.get_default_columns(), MofNCompleteColumn())

        with progress:
            task = progress.add_task("running all simulation cases...", total=len(scenarios))
            
            all_cases = []
            i = 0
            zfill = len(str(len(scenarios))) # Max. character count
            for scenario in scenarios:
                model = model_class()
                sim = Simulation(self.runs_dir / str(i).zfill(zfill) , keep_dir=True)
                for key, val in scenario.items():
                    model.set(key, val)
                model.configure(sim)
                all_cases.append(BatchResult(sim, model))
                i += 1

            def in_parallel(one_case: BatchResult):
                one_case.sim.run()
                progress.update(task, advance=1)

            pyu.parallel_for(data=all_cases, func=in_parallel)
        
        self.res = all_cases
        return all_cases
    

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

    
def plot(res: pd.DataFrame, toplot, img_dir: pathlib.Path) -> None:
    sns.set_theme()
    
    for data in toplot:
        melted = res.melt(
            id_vars=data.x,
            value_vars=data.ys,
            var_name=data.type,
            value_name=data.y_unit)
    
        ymax = melted[data.y_unit].max() * 1.3
        
        ax = sns.lineplot(melted, x=data.x, y=data.y_unit, hue=data.type)
        ax.set_title(f"{data.type}")

        if not np.isinf(ymax):
            ax.set_ylim(bottom=0, top=ymax)

        ax.set_xlabel(f"{data.x} ({data.x_unit})")
        ax.set_ylabel(f"{data.type} ({data.y_unit})")
        fig = plt.gcf()
        fig.tight_layout()

        exts = ["png", "pdf"]
        for e in exts:
            (img_dir / e).mkdir(exist_ok=True, parents=True)
            fig.savefig(img_dir / e / f"{data.type}-{data.x}.{e}")

        fig.clf()
