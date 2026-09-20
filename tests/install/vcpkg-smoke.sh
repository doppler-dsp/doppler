#!/usr/bin/env bash
# vcpkg-smoke.sh — install doppler through its in-repo vcpkg overlay port and
# consume it the way a vcpkg user does: a CMake configure that names vcpkg's
# toolchain file and a triplet, nothing else.
#
# That last part is the point. `release-smoke.sh` hands the consumer a
# CMAKE_PREFIX_PATH; a vcpkg user never writes one. They rely on the toolchain
# to find `share/doppler/doppler-config.cmake` — the location
# vcpkg_cmake_config_fixup MOVED the config to — and, on Windows, to copy
# doppler.dll beside their executable. So this passes no prefix and adds
# nothing to PATH: if the shared consumer runs, the port's layout is right.
#
# The consumer is example-projects/consumer, the same project the release
# smoke builds, so the example stays the test.
#
# Usage:
#   tests/install/vcpkg-smoke.sh [TRIPLET] [WORK_DIR]
#     TRIPLET   default: x64-windows-clangcl on Windows, else
#               <arch>-<os>-dynamic (the port is dynamic-linkage only).
#     WORK_DIR  install root + consumer build (default: a temp dir).
#
# Needs: VCPKG_ROOT (or the GitHub runners' VCPKG_INSTALLATION_ROOT) naming a
#        bootstrapped vcpkg; cmake; on Windows, clang-cl.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VCPKG_ROOT="${VCPKG_ROOT:-${VCPKG_INSTALLATION_ROOT:-}}"
[ -n "$VCPKG_ROOT" ] || {
    echo "vcpkg-smoke: set VCPKG_ROOT to a bootstrapped vcpkg checkout" >&2
    exit 2
}

# A full checkout has ports/. The vcpkg BUNDLED with Visual Studio does not:
# it is manifest-mode only, and a VS developer prompt exports VCPKG_ROOT
# pointing at it -- so the variable being set proves nothing.
[ -d "$VCPKG_ROOT/ports" ] || {
    echo "vcpkg-smoke: $VCPKG_ROOT has no ports/ -- it is not a full vcpkg" >&2
    echo "  checkout (Visual Studio's bundled copy is manifest-only and a" >&2
    echo "  developer prompt sets VCPKG_ROOT to it). Clone microsoft/vcpkg," >&2
    echo "  bootstrap it, and pass VCPKG_ROOT=<that directory>." >&2
    exit 2
}

WINDOWS=0; EXE=""
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) WINDOWS=1; EXE=".exe" ;;
esac
# cmake and vcpkg.exe are native Windows programs: give them D:/a/... rather
# than Git Bash's /d/a/..., which they would read as a path on the current
# drive.
native() { if [ "$WINDOWS" = 1 ]; then cygpath -m "$1"; else echo "$1"; fi; }

if [ -n "${1:-}" ]; then
    TRIPLET="$1"
elif [ "$WINDOWS" = 1 ]; then
    TRIPLET="x64-windows-clangcl"
else
    case "$(uname -m)" in
        x86_64) arch=x64 ;;
        aarch64|arm64) arch=arm64 ;;
        *) echo "vcpkg-smoke: no triplet for $(uname -m)" >&2; exit 2 ;;
    esac
    case "$(uname -s)" in
        Linux)  TRIPLET="$arch-linux-dynamic" ;;
        Darwin) TRIPLET="$arch-osx-dynamic" ;;
        *) echo "vcpkg-smoke: unsupported platform $(uname -s)" >&2; exit 2 ;;
    esac
fi

if [ -n "${2:-}" ]; then
    mkdir -p "$2"; work="$(cd "$2" && pwd)"
else
    work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
fi
root="$(native "$VCPKG_ROOT")"
installed="$(native "$work")/installed"
ports="$(native "$ROOT")/packaging/vcpkg/ports"
triplets="$(native "$ROOT")/packaging/vcpkg/triplets"

# ── install ──────────────────────────────────────────────────────────────────
# --binarysource=clear: the port builds the checkout in place, and vcpkg's
# package ABI hashes the PORT files, not the sources they point at. A cached
# binary would therefore survive any edit to doppler itself and this would
# test last week's build.
echo ">> vcpkg install doppler:$TRIPLET"
"$VCPKG_ROOT/vcpkg$EXE" install "doppler:$TRIPLET" \
    --vcpkg-root="$root" \
    --overlay-ports="$ports" --overlay-triplets="$triplets" \
    --x-install-root="$installed" --binarysource=clear \
    || {
        # vcpkg prints only the log's PATH; on a CI runner that path is gone
        # with the job, so the log has to be in the output to be read at all.
        for log in "$VCPKG_ROOT"/buildtrees/doppler/*.log; do
            [ -f "$log" ] || continue
            echo "──── $log" >&2; tail -60 "$log" >&2
        done
        echo "FAIL: vcpkg install doppler:$TRIPLET" >&2; exit 1
    }

# vcpkg exits 0 on a post-build lint WARNING, so the layout is asserted here
# rather than read off the exit code.
tree="$work/installed/$TRIPLET"
for want in include/doppler/lo/lo_core.h share/doppler/doppler-config.cmake \
            share/doppler/copyright share/doppler/usage; do
    [ -f "$tree/$want" ] || { echo "FAIL: no $want in $tree" >&2; exit 1; }
done
[ ! -e "$tree/lib/cmake" ] || {
    echo "FAIL: lib/cmake survived the config fixup" >&2; exit 1; }
[ ! -e "$tree/debug/include" ] || {
    echo "FAIL: debug/include was installed" >&2; exit 1; }

# ── consume ──────────────────────────────────────────────────────────────────
echo ">> CMake consumer through the vcpkg toolchain"
gen=()
if [ "$WINDOWS" = 1 ]; then
    gen=(-G Ninja -DCMAKE_C_COMPILER=clang-cl)
fi
cbuild="$work/consumer"
log="$work/step.log"
quiet() { "$@" >"$log" 2>&1 || { cat "$log" >&2; echo "FAIL: $*" >&2; exit 1; }; }
quiet cmake -S "$(native "$ROOT")/example-projects/consumer" \
    -B "$(native "$cbuild")" "${gen[@]}" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$root/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
    -DVCPKG_INSTALLED_DIR="$installed"
quiet cmake --build "$(native "$cbuild")"
for exe in consumer_shared consumer_static; do
    [ -x "$cbuild/$exe$EXE" ] || { echo "FAIL: $exe not built" >&2; exit 1; }
    "$cbuild/$exe$EXE" >/dev/null || { echo "FAIL: $exe did not run" >&2; exit 1; }
    echo "   OK: $exe ran"
done
echo ">> PASS: doppler:$TRIPLET installs from the overlay port and is"
echo "         consumable through the vcpkg toolchain, shared + static."
