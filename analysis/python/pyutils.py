# Python utilities
import jinja2
import numpy as np
import tempfile
import pathlib
import os
import decimal
import subprocess as sp
import joblib


def build_template_file(in_path: str, vars: dict):
    """
    Read a jinja2 template from `in_path` and build it into a NamedTemporaryFile which is returned.
    The temporary file's parent directory is this python file's directory.
    Params:
        vars: List of key-value variables for the jinja template. Value is stringified automatically.
    """
    # Make output temporary file in current directory
    # Needed because Docker cannot acces host's `/tmp` directory.
    out_file = tempfile.NamedTemporaryFile(dir=pathlib.Path(__file__).parent.as_posix())
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

def run_process(argv: list, each_line=print_each_line) -> None:
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
    proc = sp.Popen(argv, stdout=sp.PIPE)
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


def parallel_for(data: list, func):
    results = joblib.Parallel(n_jobs=-1)(joblib.delayed(func)(i) for i in data)
    return results