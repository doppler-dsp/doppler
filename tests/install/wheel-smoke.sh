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
#   tests/install/wheel-smoke.sh --pypi 0.49.0 [--python 3.13]
#
# --python 3.N builds the venv on that Python, so the wheel PyPI serves is that
# Python's: release.yml's smoke-pypi runs one leg per supported Python on each
# platform (doppler#1817 shipped uninstallable cp313/cp314 win_amd64 wheels
# while the smoke only ever installed cp312). The venv is checked to BE that
# Python, so a resolver that quietly picked another one cannot pass for it.
#
# --pypi retries the install itself while the index catches up with the
# publish: WHEEL_SMOKE_ATTEMPTS (default 12) tries, WHEEL_SMOKE_DELAY seconds
# apart (default 15), so 3 minutes before it fails with uv's own error.
#
# Needs: uv.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PKG="doppler-dsp"
E2E="$ROOT/deploy/validation/wfm_e2e.py"
USAGE="usage: wheel-smoke.sh --wheel-dir DIR | --pypi VERSION [--python 3.N]"

case "${1:-}" in
    --wheel-dir) MODE="wheel"; ARG="${2:?$USAGE}" ;;
    --pypi)      MODE="pypi";  ARG="${2:?$USAGE}" ;;
    *) echo "$USAGE" >&2; exit 2 ;;
esac
PYVER=""
case "${3:-}" in
    "") ;;
    --python) PYVER="${4:?$USAGE}" ;;
    *) echo "$USAGE" >&2; exit 2 ;;
esac

work="$(mktemp -d)"
trap 'rc=$?; rm -rf "$work"; \
      [ "$rc" -eq 0 ] || echo "wheel-smoke: FAILED (exit $rc)" >&2; \
      exit "$rc"' EXIT

uv venv --quiet ${PYVER:+--python "$PYVER"} "$work/venv"
# A Windows venv keeps its interpreter and console scripts in Scripts/, a POSIX
# one in bin/. Derived from what uv just made, not from the OS name, so it is
# right under any bash on Windows (Git Bash, MSYS2) without a second list.
bindir="$work/venv/bin"
[ -d "$bindir" ] || bindir="$work/venv/Scripts"
py="$bindir/python"
if [ -n "$PYVER" ]; then
    have="$("$py" -c 'import sys; print("%d.%d" % sys.version_info[:2])')"
    [ "$have" = "$PYVER" ] \
        || { echo "wheel-smoke: venv is Python $have, asked for $PYVER" >&2; exit 1; }
    echo ">> Python $have"
fi

# ── obtain and install the artifact ──────────────────────────────────────────
if [ "$MODE" = "wheel" ]; then
    ls "$ARG"/*.whl >/dev/null 2>&1 \
        || { echo "wheel-smoke: no wheel in $ARG/ — run 'make wheel'" >&2; exit 1; }
    echo ">> installing the built wheel from $ARG/"
    VIRTUAL_ENV="$work/venv" uv pip install --quiet "$ARG"/*.whl
else
    # The index lags the publish job, and the lag is per REQUEST: PyPI's CDN
    # serves /simple/ from many edges, and two requests a fraction of a second
    # apart can see different indexes. v0.51.1 is the proof (doppler#1394): a
    # `--dry-run` resolve here succeeded and the real install 0.2 s later got
    # an index without the release. So there is no separate wait any more --
    # the install IS the readiness check, retried until it succeeds or runs
    # out of attempts, and the request that sees the release is the one that
    # installs it. (--refresh because uv caches index metadata; a cached index
    # is how a post-publish check silently tests the PREVIOUS release.)
    #
    # --only-binary: with no installable wheel for this Python and platform,
    # the resolver falls back to the sdist and builds it, so the smoke would
    # pass on exactly the defect it is here to catch -- doppler#1817's
    # `cp313-cpwin_amd64` wheels, which pip skipped for the sdist.
    attempts="${WHEEL_SMOKE_ATTEMPTS:-12}"
    delay="${WHEEL_SMOKE_DELAY:-15}"
    echo ">> installing $PKG==$ARG from PyPI (a wheel, never the sdist)"
    ok=0
    for i in $(seq 1 "$attempts"); do
        if VIRTUAL_ENV="$work/venv" uv pip install --quiet --refresh \
               --only-binary "$PKG" "$PKG==$ARG" 2>"$work/uv.err"; then
            ok=1; break
        fi
        echo "   attempt $i/$attempts: not installable yet"
        [ "$i" -lt "$attempts" ] && sleep "$delay"
    done
    if [ "$ok" != 1 ]; then
        # Both causes look the same from here -- a release the index does
        # not serve yet, and one with no wheel this Python and platform can
        # install -- so say neither; uv's own words say which.
        echo "wheel-smoke: uv could not install $PKG==$ARG (a wheel only) in" \
             "$attempts attempt(s); its last error:" >&2
        cat "$work/uv.err" >&2
        exit 1
    fi
fi

# ── prove it ─────────────────────────────────────────────────────────────────
echo ">> import check (clean cwd)"
got="$(cd "$work" && "$py" -c 'import doppler; print(doppler.__version__)')"
echo "   doppler $got"
# Which wheel this leg tested, from the installed WHEEL metadata: the line a
# reader of a per-Python leg's log needs, and the evidence it was that
# Python's wheel (cp313-cp313-...) and not some other file.
( cd "$work" && "$py" -c 'import importlib.metadata as m
w = m.distribution("doppler-dsp").read_text("WHEEL") or ""
print("\n".join("   wheel " + t[4:].strip() for t in w.splitlines() if t.startswith("Tag:")))' )

# Only meaningful for --pypi: the resolver picks the version, so assert it
# picked the one being released. In --wheel mode the file IS the artifact.
if [ "$MODE" = "pypi" ] && [ "$got" != "$ARG" ]; then
    echo "wheel-smoke: installed $got, expected $ARG" >&2
    exit 1
fi

echo ">> end-to-end ($(basename "$E2E"))"
( cd "$work" && PATH="$bindir:$PATH" "$py" "$E2E" )

echo "wheel-smoke: OK — $PKG $got via --$MODE"
