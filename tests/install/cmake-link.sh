#!/usr/bin/env bash
set -euo pipefail

# --8<-- [start:shared]
gcc -o app main.c \
    -Inative/inc \
    -Lbuild -ldoppler \
    -Wl,-rpath,"$(pwd)/build"
# --8<-- [end:shared]

# --8<-- [start:static]
gcc -o app main.c \
    -Inative/inc \
    build/libdoppler.a \
    -lm
# --8<-- [end:static]
