# Analysis

For analysis, we run multiple instances of the simulation in parallel (a **batch**) by varying some parameters.

This folder permits to run easely a batch in parallel and controlling the parameters thanks to Python, in a controller class called a **model**.

The models are located in `src/models/*.py`.
Each of them is an executable Python file.
Take inspiration from them to create your own models.

## Run a model

To run the model located in `src/models/example.py`:
- Set working directory to `src`.
- Run the model with `python3 -m models.example`.

## Get the results

Each run of a model generates an unique folder in `out/`, based on the timestamp (so most recent ones appear first alphabetically).
This folder will contain:
- `runs/X/config`: All input configuration files are copied here.
- `runs/X/out/`: All output files are generated here.
  - Plus `stdout.txt`.

Where `X` is the index of the simulation. For example, if a batch contains 32 simulations, `X` will vary between `0` and `31`.