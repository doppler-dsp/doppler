#!/usr/bin/env bash
# mem-guard.sh — run a command under a memory ceiling, when the host can hold one.
#
#   scripts/mem-guard.sh <command> [args...]
#
# A runaway process on a small VM does not fail: it takes the machine down,
# and with it every process on it (three WSL kills in this repo's history,
# the last an unbounded read of /dev/full inside a C test). A cgroup ceiling
# turns that into the runaway being killed by itself, with everything else
# untouched. `systemd-run --user --scope` puts the command and all its
# children in a fresh scope with MemoryMax set, inheriting the environment,
# so a `VAR=x mem-guard.sh cmd` prefix reaches cmd unchanged.
#
# The ceiling is PROVED before it is trusted. A user manager that accepts
# MemoryMax without the memory controller delegated to it enforces nothing,
# and that would be an inert guard reporting a ceiling it does not hold. So a
# 64 MiB allocation is run under a 32 MiB ceiling first: only if that probe
# is killed is the real command run under the ceiling. Otherwise -- no
# systemd user session (a CI runner, a container, macOS), or a ceiling that
# is accepted but not enforced -- the command runs unguarded and says so on
# stderr. Either way the command runs; the guard never blocks the work.
#
#   MEM_GUARD_MAX     ceiling; default 3/4 of MemTotal (e.g. 11500M)
#   MEM_GUARD_PYTHON  interpreter for the probe; default python3
#   MEM_GUARD=0       skip the guard entirely
set -euo pipefail

[ $# -ge 1 ] || { echo "usage: $0 <command> [args...]" >&2; exit 2; }

if [ "${MEM_GUARD:-1}" = 0 ]; then
  exec "$@"
fi

max=${MEM_GUARD_MAX:-}
if [ -z "$max" ] && [ -r /proc/meminfo ]; then
  max=$(awk '/^MemTotal/ { printf "%dM", $2 * 3 / 4 / 1024 }' /proc/meminfo)
fi
py=${MEM_GUARD_PYTHON:-python3}

unguarded() {
  echo "mem-guard: $1; running unguarded" >&2
  shift
  exec "$@"
}

[ -n "$max" ] || unguarded "no MemTotal to derive a ceiling from" "$@"
command -v systemd-run >/dev/null 2>&1 \
  || unguarded "systemd-run not found" "$@"
systemd-run --user --scope -q -- true 2>/dev/null \
  || unguarded "no systemd user session" "$@"

# The proof: 64 MiB touched under a 32 MiB ceiling must die -- of SIGKILL,
# exit 137, and nothing else counts. A probe that could not run at all (no
# such interpreter) also exits non-zero, and reading that as "enforced"
# would arm a guard on a broken probe. `bytearray(b'x') * n` copies, so
# every page is written and counted. Through a function with its stderr
# redirected, so the kill is not announced by this shell as if it were the
# command's.
probe() {
  systemd-run --user --scope -q -p MemoryMax=32M -p MemorySwapMax=0 -- \
    "$py" -c "bytearray(b'x') * (64 << 20)"
}
rc=0
probe >/dev/null 2>&1 || rc=$?
[ "$rc" -eq 137 ] \
  || unguarded "probe exited $rc, not 137: ceiling not proved" "$@"

echo "mem-guard: ceiling $max" >&2
exec systemd-run --user --scope -q -p MemoryMax="$max" -p MemorySwapMax=0 \
  -- "$@"
