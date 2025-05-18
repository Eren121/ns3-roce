# Python utilities
import jinja2
import numpy as np
import tempfile
import pathlib
import os
import decimal
import subprocess as sp
import joblib
import json
from avro.io import DatumReader, DatumWriter
from avro.datafile import DataFileReader, DataFileWriter
import seaborn as sns
import pandas as pd

class NpEncoder(json.JSONEncoder):
    """
    Used internally by this module to (de)serialize Python objects containing numpy objects transparently.
    By default, some types are not supported.
    See https://stackoverflow.com/questions/50916422/python-typeerror-object-of-type-int64-is-not-json-serializable.

    Also convert paths to string.
    """
    def default(self, obj):
        if isinstance(obj, np.integer):
            return int(obj)
        elif isinstance(obj, np.floating):
            return float(obj)
        elif isinstance(obj, np.bool_):
            return bool(obj)
        elif isinstance(obj, np.ndarray):
            return obj.tolist()
        elif isinstance(obj, pathlib.Path):
            return obj.as_posix()
        
        return super().default(obj)
    

def dump_json(data, indent=4) -> str:
    """
    Serializes a Python object into a JSON string.
    """
    return json.dumps(data, cls=NpEncoder, indent=indent)

def write_to_file(file: pathlib.Path, content: str) -> None:
    """
    Writes a string to a file.
    Erases the file if the file already exists.
    """
    with open(file, "w") as text_file:
        text_file.write(content)


def read_all_file(file: pathlib.Path) -> str:
    """
    Reads all the content of a file into a string.
    """
    with open(file, "r") as text_file:
        return text_file.read()
    raise Exception(f"Cannot read file {file}")


def load_json(file: pathlib.Path) -> dict:
    """
    Reads all the content of a JSON file `.json` into a dictionary.
    """
    with open(file, "r") as text_file:
        return json.load(text_file)
    raise Exception(f"Cannot read file {file}")


def build_template_file(in_path: str, vars: dict, out_dir: str = None) -> tempfile.NamedTemporaryFile:
    """
    Builds a jinja2 template and stores the output into a temporary file.
    The steps are:
    - Read a jinja2 template from `in_path`
    - Builds the template
    - Stores it into a NamedTemporaryFile which is returned.

    Parameters:
        in_path:
            Path to the input `.jinja2` template.
        vars:
            List of key-value variables for the jinja template.
        out_dir:
            Where to generate the built template.
            If `None`, the directory of the generated file is the containing directory of this module.
    """

    # Default of `out_dir` if not specified.
    if out_dir is None:
        out_dir = pathlib.Path(__file__).parent.as_posix()

    # Ensures `out_dir` exists.
    out_dir.mkdir(parents=True, exist_ok=True)

    # Make output temporary file in current directory
    # Needed because Docker cannot acces host"s `/tmp` directory.
    out_file = tempfile.NamedTemporaryFile(dir=out_dir)
    print(f"Build template in_file='{in_path}' out_file='{out_file.name}'")

    # Jinja do not take absolute paths, so hack around.
    jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader(searchpath=in_path.parent.as_posix()))

    # Build the template
    jinja_template= jinja_env.get_template(pathlib.Path(in_path).name)
    out_content = jinja_template.render(vars)
    out_file.write(out_content.encode())
    out_file.flush() # Ensure data is written to disk

    return out_file


def print_line(line: str) -> None:
    """
    Just prints `line` to the standard output.

    Can be used as a callback for `run_process`.
    """
    print(line, end="")


def run_process(argv: list, each_line=None) -> None:
    """
    Runs a process.
    The command is stored in `argv[0]` and each argument in the rest of the elements.
    Raise an exception if the process exists with failure.

    Parameters:
        argv:
            List of arguments.
            First element is the command itself.
        each_line:
            If not None, callback to be called for each line printed by the process (includes the newline character).
            With as single argument the current line as a string.
    """
    print(f"Running {argv}")
    proc = sp.Popen(argv, stdout=sp.PIPE, stderr=sp.STDOUT)
    running = True
    while running:
        line = proc.stdout.readline()
        if not line:
            running = False
        else:
            if each_line is not None:
                each_line(line.decode("utf-8"))
    proc.wait()
    if proc.returncode != 0:
        raise Exception(f"Error: process exited with return code {proc.returncode}")


def parallel_for(func_args: list, func, jobs: int = None):
    """
    Runs a function in parallel.
    `func(func_args[0])`, `func(func_args[1])`, etc... are run in parallel.

    Parameters:
        func_args: Each element is fed as the argument of `func`, for each execution.
        func: The function to execute.
        jobs: Maximum count of parallel jobs. If none, then choose automatically.
    
    Returns:
        The array of result of each function.
    """
    # Some bugs with loky backend and rich.progress.
    # We actually don't need parallel python code,
    # because we spawn a new Process for each task.
    n_jobs = (-1 if jobs is None else jobs)
    results = joblib.Parallel(backend="threading", n_jobs=n_jobs)(
        joblib.delayed(func)(func_arg) for func_arg in func_args)
    return results


def read_avro(path: pathlib.Path) -> pd.DataFrame:
  """
  Reads an avro dataset into a pandas DataFrame.
  """
  with open(path, "rb") as file:
    reader = DataFileReader(file, DatumReader())
    return pd.DataFrame.from_records([r for r in reader])
  

def scientific_axis(g: sns.FacetGrid) -> None:
   """
   Displays the axis unit of a seaborn `FacetGrid` in scientific notation.
   """
   for axes in g.axes.flat:
      axes.ticklabel_format(axis='both', style='scientific', scilimits=(0, 0))
