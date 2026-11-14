#!/bin/sh
# The runs behind docs/design/correlator.md section 9, from the repository
# root after `make build` and `make pyext`. Prints the runs run-<commit>.txt
# holds, except its `# rerun` block (probe.py by hand at REPS=15) and the
# second bench_blue round (the binary run again).
set -e
D=docs/design/correlator-measurements
O=${TMPDIR:-/tmp}/correlator-measurements
mkdir -p "$O"
echo "main $(git rev-parse --short HEAD) $(date -u +%FT%TZ)"
cc -O3 -I native/inc -I native/src/correlator -I native/src "$D/stage.c" \
  build/libdoppler.a -lm -lpthread -o "$O/stage"
for w in 3 11; do "$O/stage" "$w"; done
for c in design wide; do
  for t in 1 2 4 8 16; do
    CASE=$c THREADS=$t REPS=5 .venv/bin/python "$D/probe.py"
  done
done
cc -O3 -fopenmp "$D/triad.c" -o "$O/triad"
"$O/triad"
cc -O3 -I native/inc -I vendor/pffft "$D/bench_blue.c" vendor/pffft/pffft.c \
  build/libdoppler.a -lm -lpthread -o "$O/bench_blue"
"$O/bench_blue"
