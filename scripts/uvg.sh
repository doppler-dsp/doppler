#!/usr/bin/env bash
# The one door every uv call the Makefile makes goes through (`UV`, which is
# `override UV = bash scripts/uvg.sh $(UV_BIN)`). It refuses one thing:
# a NESTED make asking a sync-capable uv command (run, sync, venv) to rebuild a
# .venv that is on another Python than the request names.
#
# Why (#3305): `uv run` removes and recreates a .venv whose Python disagrees
# with the one it is asked for, and the suite is still importing from it, so the
# packages vanish mid-run. A top-level make may rebuild (that is how a venv is
# made or moved); a nested make may not, and says so in one line:
#
#   venv_guard: the .venv is on Python 3.14; this request is 3.12 (from
#   .python-version). A nested make will not rebuild it. Run `make <goal>` at
#   top level, or set UV_PYTHON=3.14 to match the venv.
#
# `uv run --no-sync` never syncs, so it is always allowed (the interpreter
# probe uses it). Everything else exec's unchanged.
#
# Usage: uvg.sh <uv> [args...]
set -euo pipefail

uv_bin="$1"
shift

sub="${1:-}"
case "$sub" in
  run | sync | venv) ;;
  *) exec "$uv_bin" "$@" ;;
esac

for a in "$@"; do
  if [ "$a" = "--no-sync" ]; then
    exec "$uv_bin" "$@"
  fi
done

want="${UV_PYTHON:-}"
from="UV_PYTHON"
if [ -z "$want" ] && [ -f .python-version ]; then
  want=$(head -n 1 .python-version)
  from=".python-version"
fi

if [ -n "$want" ] && [ -f .venv/pyvenv.cfg ] && [ "${MAKELEVEL:-0}" -gt 0 ]; then
  have=$(sed -n 's/^version_info = \([0-9]*\.[0-9]*\).*/\1/p' .venv/pyvenv.cfg)
  req=$(printf '%s' "$want" | cut -d. -f1,2)
  if [ -n "$have" ] && [ "$have" != "$req" ]; then
    echo "venv_guard: the .venv is on Python $have; this request is $req" \
      "(from $from). A nested make will not rebuild it. Run the same make" \
      "command at top level, or set UV_PYTHON=$have to match the venv." >&2
    exit 2
  fi
fi

exec "$uv_bin" "$@"
