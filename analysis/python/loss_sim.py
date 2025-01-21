#!/usr/bin/env python3
#
# Run the simulation to make the graph X: loss, Y: bandwidth.

import pyutils as pyu
import project
import numpy as np
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
    out = pyu.run_process(argv=["make", "-C", project.root_path().as_posix(), "run", f"config={docker_config_path}"])


def main() -> None:
    script_dir = pathlib.Path(__file__).parent
    in_dir = script_dir / "in"
    out_dir = script_dir / "out"
    config_template_path = in_dir / "config.txt.jinja"
    for error_rate in np.linspace(0, 0.01, 10):
        config_vars = {
            "ERROR_RATE_PER_LINK": error_rate,
            "IN_DIR": get_path_rel_to_container(in_dir),
            "OUT_DIR": get_path_rel_to_container(out_dir)
        }
        config_file = pyu.build_template_file(in_path=config_template_path, vars=config_vars)
        comp_time = run_simulation(pathlib.Path(config_file.name))
        print(comp_time)


if __name__ == "__main__":
    main()