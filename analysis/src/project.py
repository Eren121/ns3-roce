import pathlib
import os
import git

"""
Get important directories of the git project ns3-roce.
Permits to convert between host paths and container paths.
"""


def root_path() -> pathlib.Path:
    """
    Gets the git root directory absolute path.
    """
    repo = git.Repo('.', search_parent_directories=True)
    return pathlib.Path(repo.working_tree_dir)


def dir_from_root(path: pathlib.Path, create: bool = True):
    """
    Converts the path of a directory from relative to the git root to an absolute path.

    Params:
        path:
            Relative path to get from the git root.
        create:
            If false, the directory should exist.
            If true, create the directory if it doesn't exist.
    """
    path = root_path() / path
    if not path.is_dir():
        if create:
            path.mkdir(parents=True)
        else:
            raise Exception(f"Path '{path}' should exist")
    return path


def analysis_path():
    """
    Gets the analysis directory.
    """
    return dir_from_root("analysis")


def out_path():
    """
    Gets the directory where to store simulation outputs.
    """
    return dir_from_root("analysis/out", create=True)


def cd_to_script_dir() -> None:
    """
    Changes the working directory to the analysis' directory containing Python top-folder module. 
    """
    abspath = os.path.abspath(__file__)
    dname = os.path.dirname(abspath)
    os.chdir(dname)


def get_path_rel_to_container(path: pathlib.Path) -> str:
    """
    Converts from a host path to a container path.
    Throws an error if the given `path` does not point into the git project.
    """
    if not path.is_relative_to(root_path()):
        raise Exception("Path should be inside git directory.")
    return os.path.relpath(path, root_path())

