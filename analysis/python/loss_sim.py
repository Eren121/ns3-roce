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


def get_path_rel_to_container(path: pathlib.Path) -> str:
    return os.path.relpath(path, project.simulation_path())

def run_simulation(config_path: pathlib.Path):
    # Working directory of ns-3 process in container is `project.simulation_path()`.
    # `project.root_path()` is mounted in container.
    if not config_path.is_relative_to(project.root_path()):
        raise Exception("Config path should be inside Docker mounted directory (inside project's directory).")
    # Convert the host path to container path.
    # Avoid `walk_up` which needs recent Python 3.12
    docker_config_path = get_path_rel_to_container(config_path)

    magic = "Elapsed simulation time: "
    sim_time = 0.0
    def capture_last_line(line):
        nonlocal sim_time
        if(line.startswith(magic)):
            tmp = line.strip()[len(magic):]
            sim_time = float(tmp)

    pyu.run_process(argv=["make", "-C", project.root_path().as_posix(), "run", f"config={docker_config_path}", "docker_interactive="],
                    each_line=capture_last_line)
    
    return sim_time

def main() -> None:
    script_dir = pathlib.Path(__file__).parent
    in_dir = script_dir / "in"
    out_dir = script_dir / "out"
    config_template_path = in_dir / "config.json.jinja"

    error_rate_col = 'error_rate'
    sim_time_col = 'sim_time'

    res_data = {
        error_rate_col: [],
        sim_time_col: []
    }

    def run_sim(error_rate: float):
        config_vars = {
            "ERROR_RATE_PER_LINK": error_rate,
            "IN_DIR": get_path_rel_to_container(in_dir),
            "OUT_DIR": get_path_rel_to_container(out_dir)
        }
        config_file = pyu.build_template_file(in_path=config_template_path, vars=config_vars)
        sim_time = run_simulation(pathlib.Path(config_file.name))
        
        print(error_rate, ",", sim_time)
        
        return {
            error_rate_col: error_rate,
            sim_time_col: sim_time
        }
    
    repeat_same = 8
    test_cases = np.repeat(np.linspace(0, 0.1, 100), repeat_same)
    
    for tmp in pyu.parallel_for(test_cases, run_sim):
        res_data[error_rate_col].append(tmp[error_rate_col])
        res_data[sim_time_col].append(tmp[sim_time_col])
    
    res = pd.DataFrame(res_data)
    sns.scatterplot(data=res,
                    x=error_rate_col,
                    y=sim_time_col)
    plt.show()

if __name__ == "__main__":
    main()