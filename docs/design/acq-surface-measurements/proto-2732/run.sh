#!/usr/bin/env bash
# The runs behind docs/design/acq-surface-measurements.md §18, on the bench
# box, from a checkout with a Release build/libdoppler.a:
#   bash docs/design/acq-surface-measurements/proto-2732/run.sh > out.txt
# Single-threaded, pinned to one of the fastest cores (CPU, default 2).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../../.." && pwd)
cpu=${CPU:-2}
out=$(mktemp -d)
for p in shared_slow ramp_grid; do
  cc -O3 -std=c99 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
    -I"$root/native/inc" "$here/$p.c" "$root/build/libdoppler.a" \
    -lm -lpthread -o "$out/$p"
done
echo "# $(date -u '+%Y-%m-%d %H:%M UTC') $(hostname) $(git -C "$root" rev-parse --short HEAD) $(cc --version | head -1)"
echo "# load before: $(cat /proc/loadavg)"
echo "## timing: no carrier (N L D I W reps)"
for g in "2046 2048 150 2 21" "2046 2048 60 2 53" "2046 2048 150 2 3" \
         "2046 2048 10 2 3" "2046 2048 150 2 1" "2046 2048 10 2 1"; do
  taskset -c "$cpu" "$out/shared_slow" $g 7
done
echo "## timing: carrier set, F = 4 (dil = fs / carrier: 10 MSa/s or 4 MSa/s at 2.2 GHz)"
for g in "2046 2048 150 2 21 7 0.004545" "2046 2048 60 2 53 7 0.001818" \
         "2046 2048 150 2 3 7 0.004545" "2046 2048 10 2 3 7 0.004545"; do
  taskset -c "$cpu" "$out/shared_slow" $g 4
done
echo "## oracle c over seeds (no carrier)"
for seed in 1 2 3 4 5; do
  for g in "2046 2048 150 2 21" "2046 2048 60 2 53" "2046 2048 10 2 3"; do
    taskset -c "$cpu" "$out/shared_slow" $g 1 0 1 "$seed" | grep oracle
  done
done
echo "## ramp loss (N D I d F center tones)"
for d in 0.004545 0.0455 0.2; do
  for F in 1 2 3 4 8; do
    taskset -c "$cpu" "$out/ramp_grid" 2046 150 2 "$d" "$F" 1 33
  done
done
for F in 1 2 4 8; do
  taskset -c "$cpu" "$out/ramp_grid" 2046 150 2 0.0455 "$F" 0 33
done
echo "# load after: $(cat /proc/loadavg)"
rm -rf "$out"
