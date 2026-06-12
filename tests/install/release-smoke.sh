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
# Each path builds and runs the examples/consumer program (so the example
# doubles as the test), and every produced binary — plus libdoppler.so itself —
# is checked to carry NO dynamic libzmq dependency (the vendored zmq is folded
# into both libraries, so consumers never link it).
#
# Usage:
#   tests/install/release-smoke.sh [VERSION] [PREFIX_DIR]
#     VERSION     release to test (default: version from pyproject.toml).
#     PREFIX_DIR  optional pre-extracted install prefix — skips the download
#                 (for validating a local `cmake --install` tree).
#
# Needs: gh (authenticated) for the download, cmake, a C compiler, and
#        ldd/otool; pkg-config is exercised when present.
set -euo pipefail

REPO="doppler-dsp/doppler"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"   # repo root (for examples/consumer)
VERSION="${1:-$(grep -m1 '^version' "$ROOT/pyproject.toml" | cut -d'"' -f2)}"
PREFIX_DIR="${2:-}"
CC="${CC:-cc}"

case "$(uname -s)" in
    Linux)  PLAT="linux-x86_64"; SHEXT="so" ;;
    Darwin) PLAT="macos-arm64";  SHEXT="dylib" ;;
    *) echo "release-smoke: unsupported platform $(uname -s)" >&2; exit 2 ;;
esac

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# ── obtain the install prefix (download the release asset, or reuse a local one)
if [ -n "$PREFIX_DIR" ]; then
    prefix="$(cd "$PREFIX_DIR" && pwd)"
    echo ">> using local prefix: $prefix"
else
    tarball="doppler-${VERSION}-${PLAT}.tar.gz"
    echo ">> downloading $tarball from release v$VERSION"
    ( cd "$work" && gh release download "v${VERSION}" -R "$REPO" -p "$tarball" )
    prefix="$work/prefix"
    mkdir -p "$prefix"
    tar -xzf "$work/$tarball" -C "$prefix"
fi

# ── locate headers, libs, pkgconfig (libdir is lib64 on manylinux, lib on macOS)
lib_so="$(find "$prefix" -name "libdoppler.${SHEXT}" | head -1)"
[ -n "$lib_so" ] || { echo "FAIL: no libdoppler.${SHEXT} in the tarball" >&2; exit 1; }
libdir="$(dirname "$lib_so")"
pcdir="$libdir/pkgconfig"
[ -f "$prefix/include/wfm/wfmgen.h" ] || { echo "FAIL: missing headers" >&2; exit 1; }
[ -f "$libdir/libdoppler.a" ]         || { echo "FAIL: no libdoppler.a" >&2; exit 1; }
echo "   prefix=$prefix  libdir=$libdir"

# ── no-zmq assertion (ldd on Linux, otool -L on macOS); pipefail-safe ─────────
deps() { if [ "$SHEXT" = dylib ]; then otool -L "$1"; else ldd "$1"; fi; }
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
echo "   libdoppler.${SHEXT}: no libzmq runtime dep"

run() {  # run a freshly built consumer and assert it has no libzmq dep
    "$1" >/dev/null || { echo "FAIL: $1 did not run" >&2; exit 1; }
    assert_no_zmq "$1"
    echo "   OK: $(basename "$1") ran, no libzmq dep"
}

# ── 1 & 2: CMake find_package (shared + static) via the example project ───────
echo ">> CMake find_package (doppler::doppler + doppler::doppler-static)"
cbuild="$work/cmake-build"
cmake -S "$ROOT/examples/consumer" -B "$cbuild" \
    -DCMAKE_PREFIX_PATH="$prefix" >/dev/null
cmake --build "$cbuild" >/dev/null
run "$cbuild/consumer_shared"
[ -x "$cbuild/consumer_static" ] || { echo "FAIL: static target not built" >&2; exit 1; }
run "$cbuild/consumer_static"

# ── 3 & 4: pkg-config (shared + static), when pkg-config is available ─────────
if command -v pkg-config >/dev/null 2>&1; then
    export PKG_CONFIG_PATH="$pcdir"
    src="$ROOT/examples/consumer/main.c"

    echo ">> pkg-config (shared)"
    # shellcheck disable=SC2046
    $CC "$src" -o "$work/pc_shared" $(pkg-config --cflags --libs doppler) \
        -Wl,-rpath,"$libdir"
    run "$work/pc_shared"

    echo ">> pkg-config --static"
    # Link the archive explicitly + the private runtime libs pkg-config reports
    # (the self-contained .a needs only -lstdc++ -lpthread -lm).
    priv="$(pkg-config --libs-only-l --static doppler | sed 's/-ldoppler//g')"
    # shellcheck disable=SC2086
    $CC "$src" -o "$work/pc_static" $(pkg-config --cflags doppler) \
        "$libdir/libdoppler.a" $priv
    run "$work/pc_static"
else
    echo ">> pkg-config not found — skipping pkg-config paths"
fi

echo ">> PASS: v$VERSION ($PLAT) consumable via find_package + pkg-config,"
echo "         static + shared, with no libzmq dependency anywhere."
