#!/usr/bin/env bash
# standalone.sh — build and run examples/standalone from the repo root.
# Doc snippets are tagged with --8<-- markers; the script is also
# directly runnable after `cmake --build build` completes.
set -euo pipefail

STANDALONE_DIR="examples/standalone"
BUILD_DIR="${1:-build}"                      # doppler build tree
OUT_DIR="examples/standalone/build"

# --8<-- [start:get-source]
git clone https://github.com/doppler-dsp/doppler
cmake -B doppler/build doppler -DCMAKE_BUILD_TYPE=Release
cmake --build doppler/build -j$(nproc)
# --8<-- [end:get-source]

# --8<-- [start:get-artifact]
cmake --install doppler/build --prefix ~/.local
export CMAKE_PREFIX_PATH=~/.local
export PKG_CONFIG_PATH=~/.local/lib/pkgconfig
# --8<-- [end:get-artifact]

# --8<-- [start:cmake-static-build-tree]
cmake -B examples/standalone/build examples/standalone \
      -DDOPPLER_BUILD_DIR=$(pwd)/build
cmake --build examples/standalone/build
./examples/standalone/build/awgn_example
# --8<-- [end:cmake-static-build-tree]

# --8<-- [start:cmake-static-installed]
cmake -B examples/standalone/build examples/standalone
cmake --build examples/standalone/build
./examples/standalone/build/awgn_example
# --8<-- [end:cmake-static-installed]

# --8<-- [start:gcc-static]
gcc -o awgn_example examples/standalone/main.c \
    -Inative/inc -Ibuild/native/inc \
    build/libdoppler.a -lm -lstdc++ -lpthread
./awgn_example
# --8<-- [end:gcc-static]

# --8<-- [start:cmake-shared-build-tree]
cmake -B examples/standalone/build examples/standalone \
      -DDOPPLER_BUILD_DIR=$(pwd)/build \
      -DDOPPLER_LINK=shared
cmake --build examples/standalone/build
./examples/standalone/build/awgn_example
# --8<-- [end:cmake-shared-build-tree]

# --8<-- [start:cmake-shared-installed]
cmake -B examples/standalone/build examples/standalone
cmake --build examples/standalone/build
./examples/standalone/build/awgn_example
# --8<-- [end:cmake-shared-installed]

# --8<-- [start:gcc-shared]
gcc -o awgn_example examples/standalone/main.c \
    -Inative/inc -Ibuild/native/inc \
    -Lbuild -ldoppler -Wl,-rpath,$(pwd)/build -lm
./awgn_example
# --8<-- [end:gcc-shared]

# --8<-- [start:python-pip]
pip install doppler-dsp
python examples/standalone/example.py
# --8<-- [end:python-pip]

# --8<-- [start:python-source]
cmake -B build -DBUILD_PYTHON=ON && cmake --build build -j$(nproc)
python examples/standalone/example.py
# --8<-- [end:python-source]
