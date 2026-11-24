#!/usr/bin/env bash
# Run a uv command for the project, but refuse to REBUILD a .venv on another
# Python from inside a nested make.
#
# Why (#3305): `uv run` removes and recreates a .venv whose Python disagrees
# with the one it is asked for ("Removed virtual environment at: .venv"). A
# nested make, or a test that runs one, does that under a suite that is still
# importing from the venv: the packages go missing mid-run. A top-level make
# with no venv, or one already on the requested Python, is unaffected.
#
# Usage: venv_guard.sh <command> [args...]
#   Exits 2 with one line naming both versions when a nested make would
#   rebuild; otherwise execs the command unchanged.
set -euo pipefail

want="${UV_PYTHON:-}"
if [ -z "$want" ] && [ -f .python-version ]; then
  want=$(head -n 1 .python-version)
fi

if [ -n "$want" ] && [ -f .venv/pyvenv.cfg ] && [ "${MAKELEVEL:-0}" -gt 0 ]; then
  have=$(sed -n 's/^version_info = \([0-9]*\.[0-9]*\).*/\1/p' .venv/pyvenv.cfg)
  req=$(printf '%s' "$want" | cut -d. -f1,2)
  if [ -n "$have" ] && [ "$have" != "$req" ]; then
    echo "venv_guard: .venv is Python $have and this make asks for $req; a" \
      "nested make refuses to rebuild it. Fix: remove .venv and run make" \
      "again, or set UV_PYTHON=$have to match the venv." >&2
    exit 2
  fi
fi

exec "$@"
