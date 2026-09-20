#!/usr/bin/env bash
# release-smoke.sh — post-release smoke test of the published C-library tarball.
#
# Downloads the `doppler-<version>-<platform>.tar.gz` asset from the GitHub
# release and validates that a downstream can actually consume it through every
# supported integration path, with only the expected dependencies:
#
#   1. CMake find_package → doppler::doppler         (shared)
#   2. CMake find_package → doppler::doppler-static  (static, self-contained)
#   3. pkg-config                                    (shared)
#   4. pkg-config --static                           (static)
#
# Each path builds and runs the example-projects/consumer program (so the example
# doubles as the test), and every produced binary — plus libdoppler.so itself —
# is checked to carry NO dynamic libzmq dependency. ZMQ has been fully removed
# from doppler; this assertion is now a permanent regression guard against it
# (or any other C++ runtime dependency) ever creeping back into the core.
#
# Usage:
#   tests/install/release-smoke.sh [VERSION] [PREFIX_DIR]
#     VERSION     release to test (default: version from pyproject.toml).
#     PREFIX_DIR  optional pre-extracted install prefix — skips the download
#                 (for validating a local `cmake --install` tree).
#
# Needs: gh (authenticated) for the download, cmake, a C compiler, and
#        ldd/otool; pkg-config is exercised when present.
#
# Windows (Git Bash, from an MSVC developer environment): the asset is a .zip,
# the consumer is built with Ninja + clang-cl (cl.exe cannot compile doppler's
# headers), dependencies are read with llvm-objdump, and the shared consumer
# finds doppler.dll through PATH. pkg-config and the stream layer are skipped
# there, each saying why: pkg-config is not how a Windows build finds a
# library, and the stream layer is not ported (#1364).
set -euo pipefail

REPO="doppler-dsp/doppler"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"   # repo root (for example-projects/consumer)
VERSION="${1:-$(grep -m1 '^version' "$ROOT/pyproject.toml" | cut -d'"' -f2)}"
PREFIX_DIR="${2:-}"
CC="${CC:-cc}"

case "$(uname -s)" in
    Linux)  SHEXT="so" ;;
    Darwin) SHEXT="dylib" ;;
    MINGW*|MSYS*|CYGWIN*) SHEXT="dll" ;;
    *) echo "release-smoke: unsupported platform $(uname -s)" >&2; exit 2 ;;
esac
WINDOWS=0; EXE=""; EXT="tar.gz"
if [ "$SHEXT" = dll ]; then WINDOWS=1; EXE=".exe"; EXT="zip"; fi

case "$(uname -s)/$(uname -m)" in
    Linux/x86_64)               PLAT="linux-x86_64" ;;
    # aarch64 is the conventional Linux uname -m for this architecture, but
    # some environments report arm64 instead -- same physical architecture.
    Linux/aarch64|Linux/arm64)  PLAT="linux-aarch64" ;;
    Darwin/arm64)                PLAT="macos-arm64" ;;
    MINGW*/x86_64|MSYS*/x86_64|CYGWIN*/x86_64)  PLAT="windows-x86_64" ;;
    *) echo "release-smoke: no published tarball for $(uname -s)/$(uname -m)" >&2; exit 2 ;;
esac

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# ── obtain the install prefix (download the release asset, or reuse a local one)
if [ -n "$PREFIX_DIR" ]; then
    prefix="$(cd "$PREFIX_DIR" && pwd)"
    echo ">> using local prefix: $prefix"
else
    tarball="doppler-${VERSION}-${PLAT}.${EXT}"
    echo ">> downloading $tarball from release v$VERSION"
    ( cd "$work" && gh release download "v${VERSION}" -R "$REPO" -p "$tarball" )
    prefix="$work/prefix"
    mkdir -p "$prefix"
    # cmake reads both formats; Git Bash has no unzip.
    ( cd "$prefix" && cmake -E tar xf "$work/$tarball" )
fi

# ── locate headers, libs, pkgconfig (libdir is lib64 on manylinux, lib on macOS)
# Windows names them differently: the DLL is bin/doppler.dll, lib/doppler.lib is
# its import library, and the static library is lib/doppler_static.lib.
if [ "$WINDOWS" = 1 ]; then
    shname="doppler.dll"; stname="doppler_static.lib"
else
    shname="libdoppler.${SHEXT}"; stname="libdoppler.a"
fi
lib_so="$(find "$prefix" -name "$shname" | head -1)"
[ -n "$lib_so" ] || { echo "FAIL: no $shname in the archive" >&2; exit 1; }
libdir="$prefix/lib"
[ "$WINDOWS" = 1 ] || libdir="$(dirname "$lib_so")"
pcdir="$libdir/pkgconfig"
[ -f "$prefix/include/doppler/lo/lo_core.h" ] || { echo "FAIL: missing headers" >&2; exit 1; }
[ -f "$libdir/$stname" ]              || { echo "FAIL: no $stname" >&2; exit 1; }
echo "   prefix=$prefix  libdir=$libdir"

# ── no-zmq assertion (ldd on Linux, otool -L on macOS); pipefail-safe ─────────
deps() {
    case "$SHEXT" in
        dylib) otool -L "$1" ;;
        dll)   llvm-objdump -p "$1" | grep 'DLL Name' ;;
        *)     ldd "$1" ;;
    esac
}
assert_no_zmq() {
    local hits
    hits="$(deps "$1" | grep -i zmq || true)"
    if [ -n "$hits" ]; then
        echo "FAIL: $1 carries a dynamic libzmq dependency:" >&2
        echo "$hits" >&2
        exit 1
    fi
}

assert_no_zmq "$lib_so"
echo "   $shname: no libzmq runtime dep"

run() {  # run a freshly built consumer and assert it has no libzmq dep
    "$1" >/dev/null || { echo "FAIL: $1 did not run" >&2; exit 1; }
    assert_no_zmq "$1"
    echo "   OK: $(basename "$1") ran, no libzmq dep"
}

# ── 1 & 2: CMake find_package (shared + static) via the example project ───────
echo ">> CMake find_package (doppler::doppler + doppler::doppler-static)"
cbuild="$work/cmake-build"
gen=()
if [ "$WINDOWS" = 1 ]; then
    gen=(-G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release)
    # How the shared consumer finds the DLL. PATH is colon-separated, so it
    # needs the POSIX spelling (/d/a/...): the `D:/a/...` form pwd can return
    # here splits at the drive colon.
    export PATH="$(cygpath -u "$prefix")/bin:$PATH"
fi
# Quiet on success, the whole log on failure: a smoke that fails with its
# output thrown away says only "exit 2".
quiet() {
    local log="$work/step.log"
    "$@" >"$log" 2>&1 || { cat "$log" >&2; echo "FAIL: $*" >&2; exit 1; }
}
quiet cmake -S "$ROOT/example-projects/consumer" -B "$cbuild" "${gen[@]}" \
    -DCMAKE_PREFIX_PATH="$prefix"
quiet cmake --build "$cbuild"
run "$cbuild/consumer_shared$EXE"
[ -x "$cbuild/consumer_static$EXE" ] || { echo "FAIL: static target not built" >&2; exit 1; }
run "$cbuild/consumer_static$EXE"

# ── 3 & 4: pkg-config (shared + static), when pkg-config is available ─────────
if [ "$WINDOWS" = 1 ]; then
    echo ">> pkg-config: skipped on Windows — find_package above is the"
    echo "   Windows face (the .pc file still ships, untested here)"
elif command -v pkg-config >/dev/null 2>&1; then
    export PKG_CONFIG_PATH="$pcdir"
    src="$ROOT/example-projects/consumer/main.c"

    echo ">> pkg-config (shared)"
    # shellcheck disable=SC2046
    $CC "$src" -o "$work/pc_shared" $(pkg-config --cflags --libs doppler) \
        -Wl,-rpath,"$libdir"
    run "$work/pc_shared"

    echo ">> pkg-config --static"
    # Link the archive explicitly + the private runtime libs pkg-config reports
    # (the pure-C core .a needs -lm and -lpthread — no C++ runtime, no zmq).
    priv="$(pkg-config --libs-only-l --static doppler | sed 's/-ldoppler//g')"
    # shellcheck disable=SC2086
    $CC "$src" -o "$work/pc_static" $(pkg-config --cflags doppler) \
        "$libdir/libdoppler.a" $priv
    run "$work/pc_static"
else
    echo ">> pkg-config not found — skipping pkg-config paths"
fi

# ── 5: the stream component, three ways (cc / CMake / pkg-config) ────────────
# Builds the core+stream consumer via every face and asserts identical
# output — the docs' "Compile it — three ways" snippets are --8<-- included
# from the very script this runs.
if [ "$WINDOWS" = 1 ]; then
    echo ">> stream consumer: skipped — the stream layer is not built on"
    echo "   Windows (#1364)"
    echo ">> PASS: v$VERSION ($PLAT) consumable via find_package, static +"
    echo "         shared (core), no libzmq anywhere."
    exit 0
fi
echo ">> stream consumer, three ways"
bash "$ROOT/tests/install/stream-consumer/build-three-ways.sh" "$prefix"

echo ">> PASS: v$VERSION ($PLAT) consumable via find_package + pkg-config,"
echo "         static + shared (core and stream), no libzmq anywhere."
