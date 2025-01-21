import pathlib
import os
import git


def root_path() -> pathlib.Path:
    """Get the git root directory."""
    repo = git.Repo('.', search_parent_directories=True)
    return pathlib.Path(repo.working_tree_dir)


def simulation_path() -> pathlib.Path:
    """Get the root's "simulation/" directory."""
    path = root_path() / "simulation"
    if not path.is_dir():
        raise Exception(f"Path '{path}' should exist")
    return path
    

def cd_to_script_dir() -> None:
    abspath = os.path.abspath(__file__)
    dname = os.path.dirname(abspath)
    os.chdir(dname)
    