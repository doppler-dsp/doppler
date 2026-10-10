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
# kills the largest of them instead of the VM. The slice's ceiling is ALWAYS
# the machine-wide value, 3/4 of MemTotal, never a caller's: it used to come
# from whichever guarded command started last, so one caller's
# MEM_GUARD_MAX=4G held every other session's live commands to 4G (#1960).
# A caller who wants less gets it on its OWN scope (MemoryMax on the
# systemd-run scope, nested under the shared slice), which tightens that
# command and nobody else. scripts/check_mem_guarded.py fails `make lint`
# when a parallel pytest or a zensical command is reachable without this
# script.
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
# The probe has its OWN slice, doppler-mgprobe.slice, a SIBLING of the guard
# under doppler.slice. systemd nests slices by dash, so a
# doppler-guard-probe.slice was a child of the guard, and every probe kill
# was logged against doppler-guard.slice too, making the journal useless as
# evidence of a real kill (#1961).
#
#   MEM_GUARD_MAX     a lower ceiling for THIS command's own scope (e.g.
#                     4G); the shared slice stays at 3/4 of MemTotal. A
#                     value systemd-run refuses (4GB) is dropped with a
#                     warning, and the shared ceiling alone applies
#   MEM_GUARD_PYTHON  interpreter for the probe; default python3
#   MEM_GUARD=0       skip the guard entirely
set -euo pipefail

[ $# -ge 1 ] || { echo "usage: $0 <command> [args...]" >&2; exit 2; }

if [ "${MEM_GUARD:-1}" = 0 ]; then
  exec "$@"
fi

# The shared slice's ceiling: machine-wide, the same for every caller.
max=
if [ -r /proc/meminfo ]; then
  max=$(awk '/^MemTotal/ { printf "%dM", $2 * 3 / 4 / 1024 }' /proc/meminfo)
fi
# This command's own ceiling, if it asked for one.
cap=${MEM_GUARD_MAX:-}
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
# command's. It runs under a SLICE ceiling, its own, because the shared slice
# ceiling is what every guarded command is held to. A caller's MEM_GUARD_MAX
# adds a per-scope ceiling nested under that slice, and the probe does not
# prove that one separately: it can only lower a ceiling that is proved, so
# where it is inert the shared ceiling still holds.
#
# OOMPolicy=continue keeps the expected kill from being reported as a failure:
# the kernel still kills the probe (exit 137, which is the proof), but systemd
# logs "killed some processes in this unit" instead of marking the scope
# `Failed with result 'oom-kill'`. Without it a desktop session raises a
# "terminated because the system is low on memory" popup for EVERY guarded
# command -- 60 of them on one box, none a real shortage. A scope accepts
# OOMPolicy only from systemd 253 (systemd.scope(5)); an older one refuses the
# unit outright, so the probe is retried without it. The popup is the cost
# there, never the ceiling: a guard disarmed to stay quiet would be worse.
probe() {
  systemctl --user set-property --runtime doppler-mgprobe.slice \
    MemoryMax=32M MemorySwapMax=0 || return 1
  systemd-run --user --scope -q --slice=doppler-mgprobe.slice "$@" -- \
    "$py" -c "bytearray(b'x') * (64 << 20)"
}
rc=0
probe -p OOMPolicy=continue >/dev/null 2>&1 || rc=$?
if [ "$rc" -ne 137 ]; then
  rc=0
  probe >/dev/null 2>&1 || rc=$?
fi
[ "$rc" -eq 137 ] \
  || unguarded "probe exited $rc, not 137: ceiling not proved" "$@"

slice=doppler-guard.slice
systemctl --user set-property --runtime "$slice" \
  MemoryMax="$max" MemorySwapMax=0 \
  || unguarded "could not set the ceiling on $slice" "$@"
# A cap systemd-run cannot take (MEM_GUARD_MAX=4GB, any typo) fails the
# exec below, and then the command never runs. So it is tried on `true`
# first, and a refused one is dropped for the shared ceiling alone. That is
# still a guarded run: `unguarded` here would throw away a ceiling that has
# just been proved, over a bad request to go lower.
if [ -n "$cap" ] \
  && ! systemd-run --user --scope -q --slice="$slice" \
    -p MemoryMax="$cap" -p MemorySwapMax=0 -- true >/dev/null; then
  echo "mem-guard: systemd-run would not run under MemoryMax=$cap" \
    "(MEM_GUARD_MAX); falling back to the shared ceiling alone" >&2
  cap=
fi
if [ -n "$cap" ]; then
  # Nested: the scope can never exceed the slice, so a cap above the shared
  # ceiling simply leaves the shared ceiling in force.
  echo "mem-guard: ceiling $max, shared across $slice; this command" \
    "capped at the lower of $cap and $max" >&2
  exec systemd-run --user --scope -q --slice="$slice" \
    -p MemoryMax="$cap" -p MemorySwapMax=0 -- "$@"
fi
echo "mem-guard: ceiling $max, shared across $slice" >&2
exec systemd-run --user --scope -q --slice="$slice" -- "$@"
