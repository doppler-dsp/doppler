#!/usr/bin/env bash
# wheel-smoke.sh — install a doppler-dsp wheel into a throwaway venv and prove
# it actually works, from a clean cwd.
#
# ONE primitive with two faces, because the only thing that differs between
# them is where the wheel comes from:
#
#   --wheel-dir DIR   the wheel this build just produced   (pre-publish)
#   --pypi VERSION    the wheel PyPI is serving to users   (post-publish)
#
# The pre-publish face is what release.yml's `smoke-wheel` runs before the
# upload. The post-publish face is what `smoke-pypi` runs after it, and it is
# the ONLY thing in a release that installs from the index: `smoke-wheel`
# installs a local file, and `release-watch.sh` queries the PyPI metadata
# endpoint, which says a version is LISTED, not that it installs (#1347).
#
# They are one script rather than two because "throwaway venv, install, import,
# run the e2e" is a single primitive; a second copy would drift from this one
# the first time either side is fixed.
#
# Why a throwaway venv and a clean cwd: `import doppler` must resolve to the
# INSTALLED artifact and never to ./src.
#
# Why the venv's bin/ goes on PATH and not merely its python: two of the e2e's
# checks shell out to the `wfmgen` CONSOLE SCRIPT, and calling venv/bin/python
# directly leaves that off PATH. Running the target before wiring it is what
# caught that — it read as "the wheel is broken" (8/10) when it was the
# harness.
#
# Usage:
#   tests/install/wheel-smoke.sh --wheel-dir dist
#   tests/install/wheel-smoke.sh --pypi 0.49.0
#
# Needs: uv (the --pypi readiness wait resolves through uv itself).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PKG="doppler-dsp"
E2E="$ROOT/deploy/validation/wfm_e2e.py"

case "${1:-}" in
    --wheel-dir) MODE="wheel"; ARG="${2:?usage: wheel-smoke.sh --wheel-dir DIR}" ;;
    --pypi)      MODE="pypi";  ARG="${2:?usage: wheel-smoke.sh --pypi VERSION}" ;;
    *) echo "usage: wheel-smoke.sh --wheel-dir DIR | --pypi VERSION" >&2; exit 2 ;;
esac

work="$(mktemp -d)"
trap 'rc=$?; rm -rf "$work"; \
      [ "$rc" -eq 0 ] || echo "wheel-smoke: FAILED (exit $rc)" >&2; \
      exit "$rc"' EXIT

uv venv --quiet "$work/venv"
# A Windows venv keeps its interpreter and console scripts in Scripts/, a POSIX
# one in bin/. Derived from what uv just made, not from the OS name, so it is
# right under any bash on Windows (Git Bash, MSYS2) without a second list.
bindir="$work/venv/bin"
[ -d "$bindir" ] || bindir="$work/venv/Scripts"
py="$bindir/python"

# ── obtain and install the artifact ──────────────────────────────────────────
if [ "$MODE" = "wheel" ]; then
    ls "$ARG"/*.whl >/dev/null 2>&1 \
        || { echo "wheel-smoke: no wheel in $ARG/ — run 'make wheel'" >&2; exit 1; }
    echo ">> installing the built wheel from $ARG/"
    VIRTUAL_ENV="$work/venv" uv pip install --quiet "$ARG"/*.whl
else
    # The index lags the publish job, and the lag is per ENDPOINT. This used
    # to wait on the per-version JSON API and then install through uv, which
    # reads the /simple/ index behind PyPI's CDN. They disagree for seconds
    # after a publish: on v0.51.0 the JSON said "0.51.0" 0.1 s into the wait,
    # uv answered "there is no version of doppler-dsp==0.51.0", and the
    # aarch64 job running the same steps two seconds later passed. So the wait
    # asks the question the install needs, through the tool that will answer
    # it: can uv resolve this exact version from the live index yet?
    # (--refresh because uv caches index metadata; a cached index is how a
    # post-publish check silently tests the PREVIOUS release.)
    echo ">> waiting for uv to resolve $PKG==$ARG from PyPI"
    ok=0
    for _ in $(seq 1 12); do
        if VIRTUAL_ENV="$work/venv" uv pip install --quiet --dry-run \
               --refresh "$PKG==$ARG" >/dev/null 2>&1; then
            ok=1; break
        fi
        sleep 15
    done
    [ "$ok" = 1 ] \
        || { echo "wheel-smoke: uv cannot resolve $PKG==$ARG after 3 min" >&2; exit 1; }
    echo ">> installing $PKG==$ARG from PyPI"
    VIRTUAL_ENV="$work/venv" uv pip install --quiet --refresh "$PKG==$ARG"
fi

# ── prove it ─────────────────────────────────────────────────────────────────
echo ">> import check (clean cwd)"
got="$(cd "$work" && "$py" -c 'import doppler; print(doppler.__version__)')"
echo "   doppler $got"

# Only meaningful for --pypi: the resolver picks the version, so assert it
# picked the one being released. In --wheel mode the file IS the artifact.
if [ "$MODE" = "pypi" ] && [ "$got" != "$ARG" ]; then
    echo "wheel-smoke: installed $got, expected $ARG" >&2
    exit 1
fi

echo ">> end-to-end ($(basename "$E2E"))"
( cd "$work" && PATH="$bindir:$PATH" "$py" "$E2E" )

echo "wheel-smoke: OK — $PKG $got via --$MODE"
