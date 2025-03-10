import pathlib
import os
import git

"""
Get important directories of the git project
And convert to container paths
"""


def dir_from_root(path: pathlib.Path, create: bool = True):
    """
    Get a directory from the git root.
    The directory should exist.
    Params:
        path: Path relative to the git root.
    """
    path = root_path() / path
    if not path.is_dir():
        if create:
            path.mkdir(parents=True)
        else:
            raise Exception(f"Path '{path}' should exist")
    return path


def analysis_path():
    return dir_from_root("analysis")


def out_path():
    return dir_from_root("analysis/out", create=True)


def root_path() -> pathlib.Path:
    """Get the git root directory."""
    repo = git.Repo('.', search_parent_directories=True)
    return pathlib.Path(repo.working_tree_dir)


def cd_to_script_dir() -> None:
    abspath = os.path.abspath(__file__)
    dname = os.path.dirname(abspath)
    os.chdir(dname)


def get_path_rel_to_container(path: pathlib.Path) -> str:
    """
    Convert a host path to a container path.
    The path should be mounted in the container.
    Root of the git project is mounted in container.
    """
    if not path.is_relative_to(root_path()):
        raise Exception("Path should be inside git directory.")
    return os.path.relpath(path, root_path())

