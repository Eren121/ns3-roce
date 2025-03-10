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


class NpEncoder(json.JSONEncoder):
    """https://stackoverflow.com/questions/50916422/python-typeerror-object-of-type-int64-is-not-json-serializable"""
    def default(self, obj):
        if isinstance(obj, np.integer):
            return int(obj)
        if isinstance(obj, np.floating):
            return float(obj)
        if isinstance(obj, np.bool_):
            return bool(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        return super().default(obj)
    

def dump_json(data, indent=4):
    return json.dumps(data, cls=NpEncoder, indent=indent)

def write_to_file(file: pathlib.Path, content: str) -> None:
    with open(file, "w") as text_file:
        text_file.write(content)


def read_all_file(file: pathlib.Path) -> str:
    with open(file, "r") as text_file:
        return text_file.read()
    raise Exception(f"Cannot read file {file}")


def load_json(file: pathlib.Path) -> str:
    with open(file, "r") as text_file:
        return json.load(text_file)
    raise Exception(f"Cannot read file {file}")


def build_template_file(in_path: str, vars: dict, out_dir: str = None):
    """
    Read a jinja2 template from `in_path` and build it into a NamedTemporaryFile which is returned.
    The temporary file parent directory is this python file directory.
    Params:
        vars: List of key-value variables for the jinja template. Value is stringified automatically.
    """

    if out_dir is None:
        out_dir = pathlib.Path(__file__).parent.as_posix()

    out_dir.mkdir(parents=True, exist_ok=True)

    # Make output temporary file in current directory
    # Needed because Docker cannot acces host"s `/tmp` directory.
    out_file = tempfile.NamedTemporaryFile(dir=out_dir)
    print(f"Build template in_file='{in_path}' out_file='{out_file.name}'")
    # Jinja do not take absolute path, so hack around.
    jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader(searchpath=in_path.parent.as_posix()))
    jinja_template= jinja_env.get_template(pathlib.Path(in_path).name)
    out_content = jinja_template.render(vars)
    out_file.write(out_content.encode())
    out_file.flush() # Ensure data is written to disk

    return out_file


def print_each_line(line: str) -> None:
    print(line, end="")


def run_process(argv: list, each_line=None) -> None:
    """
    Run a process.
    Raise an exception if the process exists with failure.
    Params:
        argv: List of arguments. First element is the command itself.
        each_line: If not None, callback to be called for each line printed by the process (includes the newline character).
    Returns:
        None.
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


def parallel_for(data: list, func, jobs: int = 0):
    # Some bugs with loky backend and rich.progress.
    # We actually don't need parallel python code,
    # because we spawn a new Process for each task.
    results = joblib.Parallel(backend="threading", n_jobs=(-1 if jobs <= 0 else jobs))(joblib.delayed(func)(i) for i in data)
    return results