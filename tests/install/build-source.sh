#!/usr/bin/env bash
set -euo pipefail

# --8<-- [start:make]
make
make pyext
# --8<-- [end:make]

# --8<-- [start:cmake]
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
# --8<-- [end:cmake]

# --8<-- [start:cmake-python]
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PYTHON=ON \
    -DPython3_EXECUTABLE="$(which python3)"
cmake --build build --parallel
# --8<-- [end:cmake-python]
