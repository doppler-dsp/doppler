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
# The ceiling is SHARED. Every guarded command lands in one systemd slice,
# doppler-guard.slice, and the ceiling is set on the slice, not on each
# scope: two commands under their own 3/4-of-RAM caps can each stay legal and
# still sink the machine together, which is exactly how the VM died on
# 2026-10-01 and again on 2026-10-03 -- the docs build (6.0 GiB) beside an
# xdist pytest run. In one slice they share one budget, and an overrun
# kills the largest of them instead of the VM. The last guarded command to
# start sets the slice's ceiling; with the default that is the same value
# every time. scripts/check_mem_guarded.py fails `make lint` when a parallel
# pytest or a zensical command is reachable without this script.
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
# command's. It runs under a SLICE ceiling, its own, because a slice ceiling
# is what the command will be held to: proving a per-scope one would prove a
# mechanism this script no longer uses.
probe() {
  systemctl --user set-property --runtime doppler-guard-probe.slice \
    MemoryMax=32M MemorySwapMax=0 || return 1
  systemd-run --user --scope -q --slice=doppler-guard-probe.slice -- \
    "$py" -c "bytearray(b'x') * (64 << 20)"
}
rc=0
probe >/dev/null 2>&1 || rc=$?
[ "$rc" -eq 137 ] \
  || unguarded "probe exited $rc, not 137: ceiling not proved" "$@"

slice=doppler-guard.slice
systemctl --user set-property --runtime "$slice" \
  MemoryMax="$max" MemorySwapMax=0 \
  || unguarded "could not set the ceiling on $slice" "$@"
echo "mem-guard: ceiling $max, shared across $slice" >&2
exec systemd-run --user --scope -q --slice="$slice" -- "$@"
