#!/bin/bash
# Pixi activation script for shannon.
# Sourced automatically by `pixi run` / `pixi shell`.

export SHANNON_ROOT="$PIXI_PROJECT_ROOT"

# Locally built plugins first, then installed plugins from the pixi env.
export PHLEX_PLUGIN_PATH="$PIXI_PROJECT_ROOT/build:${CONDA_PREFIX}/lib${PHLEX_PLUGIN_PATH:+:$PHLEX_PLUGIN_PATH}"
export LD_LIBRARY_PATH="$PIXI_PROJECT_ROOT/build${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# shipgeometry installs geometry DBs under $CONDA_PREFIX/share/geometry/.
# geometry_geomodel_provider resolves bare db_file names via this variable.
export SHIPGEOMETRY_ROOT="${SHIPGEOMETRY_ROOT:-$CONDA_PREFIX}"

export LD_LIBRARY_PATH="$PIXI_PROJECT_ROOT/.local-deps/data-model/lib:$LD_LIBRARY_PATH"
