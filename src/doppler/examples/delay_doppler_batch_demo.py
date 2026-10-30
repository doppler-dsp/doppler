"""delay_doppler_batch_demo.py — many delay-Doppler maps in one call.

A receiver searching a carrier offset tries one frame under several
hypotheses and keeps the best. Here a code delayed by 37 samples arrives on
a carrier 2.4 code bins away from nominal. Each hypothesis removes one
candidate offset (0, 0.5, ... 3.5 bins) from a copy of the frame, the copies
go to ``DelayDoppler`` as ONE batch, and each map's own peak-to-mean picks
the hypothesis that left the signal most compact.

Why there is a search at all: a carrier left uncorrected within each period
costs the code correlation its Dirichlet factor. Near a whole code bin the
peak sinks under the code's sidelobes and the map does not even find the
delay; within half a bin the loss is at most 3.9 dB
(``docs/design/corr2d-hypotheses.md`` section 3).

``execute_peaks`` ranks the hypotheses by each map's own statistics.
``execute_power_max`` keeps, cell by cell, the strongest map and which one it
was. Neither touches the object's dwell. The C twin is
``native/examples/delay_doppler_batch_demo.c``.

Run:
  python delay_doppler_batch_demo.py
"""

# --8<-- [start:setup]
import numpy as np

from doppler.spectral import DelayDoppler

NX, NY, DELAY = 128, 16, 37  # code length, periods per frame, code delay
OFFSET_BINS = 2.4  # true carrier offset, in code bins (1 / NX cycles/sample)
STEP_BINS, H = 0.5, 8  # hypothesis spacing and count

rng = np.random.default_rng(2683)
code = rng.choice(np.array([-1.0, 1.0]), NX).astype(np.complex64)

m = np.arange(NY * NX)
rx = np.tile(np.roll(code, DELAY), NY) * np.exp(
    2j * np.pi * OFFSET_BINS / NX * m
)
removed = STEP_BINS * np.arange(H)
batch = np.concatenate(
    [rx * np.exp(-2j * np.pi * r / NX * m) for r in removed]
).astype(np.complex64)
# --8<-- [end:setup]

# --8<-- [start:search]
dd = DelayDoppler(code, ny=NY, nthreads=0)  # 0: every core
peak, row, col, mean = dd.execute_peaks(batch)
ratio = peak / mean
best = int(np.argmax(ratio))

print("hypothesis  removed (bins)  peak/mean  delay")
for h in range(H):
    print(
        f"  {h:2d}          {removed[h]:4.1f}        {ratio[h]:8.1f}   "
        f"{col[h]:3d}"
    )
print(
    f"best: hypothesis {best} ({removed[best]:.1f} bins removed; "
    f"true offset {OFFSET_BINS})"
)

power, which = dd.execute_power_max(batch)
cell = int(np.argmax(power))
print(
    f"strongest cell over all maps: delay {cell % NX}, "
    f"from hypothesis {which[cell]}"
)
# --8<-- [end:search]

# The hypothesis nearest the true offset wins, both ways of asking agree,
# every hypothesis within half a bin finds the delay, and the dwell never
# moved: the batch is the search H execute() calls would make.
assert best == round(OFFSET_BINS / STEP_BINS), best
assert which[cell] == best and cell % NX == DELAY
near = np.abs(OFFSET_BINS - removed) < 0.5
assert np.all(col[near] == DELAY), col
assert dd.count == 0
print("OK")
