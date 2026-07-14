#!/usr/bin/env bash
# build-three-ways.sh — build the same consumer (app.c: core FFT + one
# dp_pub_* call) against an installed doppler prefix via all three
# supported faces, run all three binaries in the same environment, and
# assert their stdout is identical:
#
#   1. bare cc          (static archives, self-contained binary)
#   2. CMake            (find_package(doppler) + doppler::stream)
#   3. pkg-config       (one module name: doppler_stream)
#
# The marked snippet regions are --8<-- included by the docs (quickstart
# "Compile it — three ways"), so the commands readers copy are the exact
# commands this script executes in CI (ci.yml Build job against a local
# `cmake --install` tree; release-smoke.sh against the published tarball
# on linux-x86_64, linux-aarch64, and macos-arm64).
#
# Usage: build-three-ways.sh PREFIX
set -euo pipefail

PREFIX="${1:?usage: build-three-ways.sh PREFIX}"
PREFIX="$(cd "$PREFIX" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
CC="${CC:-cc}"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp "$HERE/app.c" "$HERE/CMakeLists.txt" "$work/"
cd "$work"

echo "== face 1: bare cc (static) =="
# --8<-- [start:cc]
cc app.c -I "$PREFIX/include" \
   "$PREFIX/lib/libdoppler_stream.a" \
   "$PREFIX/lib/libdoppler.a" \
   -lm -lpthread -o app
# --8<-- [end:cc]
mv app app-cc

echo "== face 2: CMake (find_package) =="
# --8<-- [start:cmake-commands]
cmake -B build -DCMAKE_PREFIX_PATH="$PREFIX"
cmake --build build
# --8<-- [end:cmake-commands]
cp build/app app-cmake

echo "== face 3: pkg-config =="
# --8<-- [start:pkg-config]
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
cc app.c $(pkg-config --cflags --libs doppler_stream) -lm -o app
# --8<-- [end:pkg-config]
mv app app-pc

# CMake/pkg-config faces link the shared libs; point the loader at the
# prefix so all three run in one identical environment.
export LD_LIBRARY_PATH="$PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DYLD_LIBRARY_PATH="$PREFIX/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"

./app-cc     > out-cc.txt
./app-cmake  > out-cmake.txt
./app-pc     > out-pc.txt

diff out-cc.txt out-cmake.txt
diff out-cc.txt out-pc.txt
echo "three faces OK — identical output:"
sed 's/^/  /' out-cc.txt
