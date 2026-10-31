"""delay_doppler_bank_demo.py — find a code on an unknown carrier.

A code delayed by 37 samples arrives on a carrier 1.3 code bins off
nominal, in noise. A delay-Doppler map corrects no carrier within a code
period, so a whole-bin offset puts the correlation into its Dirichlet null
and a fraction of a bin costs up to 3.9 dB. The bank searches the carrier:
``bank_detect`` makes one map per carrier on a coarse grid (half bins),
picks the candidates, refines each at its own cell on a fine grid (quarter
bins), and keeps what clears the confirm threshold. Every carrier shares
the frame's row transforms, so the coarse search costs half the fine one,
and the refine recovers what the coarse grid's straddle lost
(``docs/design/corr2d-hypotheses.md`` section 6).

The thresholds come from noise-only frames, as a detector's must. The C twin
is ``native/examples/delay_doppler_bank_demo.c``.

Run:
  python delay_doppler_bank_demo.py
"""

# --8<-- [start:setup]
import numpy as np

from doppler.spectral import DelayDoppler

NX, NY, DELAY = 128, 16, 37  # code length, periods per frame, code delay
CARRIER_BINS = 1.3  # the carrier, in code bins (1 / NX cycles/sample)
SNR_DB = -15.0  # per sample; the map's gain is NY * NX = 33 dB
NO_RATES = np.empty(0, np.complex64)

rng = np.random.default_rng(2683)
code = rng.choice(np.array([-1.0, 1.0]), NX).astype(np.complex64)


def noise() -> np.ndarray:
    n = rng.standard_normal(NY * NX) + 1j * rng.standard_normal(NY * NX)
    return (n / np.sqrt(2)).astype(np.complex64)


m = np.arange(NY * NX)
signal = np.tile(np.roll(code, DELAY), NY) * np.exp(
    2j * np.pi * CARRIER_BINS / NX * m
)
x = (10 ** (SNR_DB / 20) * signal + noise()).astype(np.complex64)
# --8<-- [end:setup]

# --8<-- [start:search]
dd = DelayDoppler(code, ny=NY)
COARSE = (-4, 5)  # carriers g / (2 NX): -2 to +2.5 bins, on half bins

# Thresholds from noise alone: the strongest coarse cell in 50 noise frames,
# with margin, for the candidate; the confirm a little above it.
floor = max(
    float(dd.bank_power_max(noise(), 2, *COARSE, NO_RATES)[0].max())
    for _ in range(50)
)
t_candidate, t_confirm = 1.2 * floor, 1.5 * floor

power, g, rate, row, col = dd.bank_detect(
    x, 2, 4, *COARSE, t_candidate, t_confirm, NO_RATES
)
coarse_peak = float(dd.bank_power_max(x, 2, *COARSE, NO_RATES)[0].max())

print("record  delay  Doppler row  carrier (bins)  refined / coarse peak")
for k in range(len(power)):
    print(
        f"  {k}      {col[k]:3d}      {row[k]:3d}          {g[k] / 4:+5.2f}"
        f"           {power[k] / coarse_peak:.3f}"
    )
# --8<-- [end:search]

# One signal, one record: at the code's delay, on the quarter-bin carrier
# nearest 1.3 bins, and stronger than the coarse grid saw it.
assert len(power) == 1, power
assert int(col[0]) == DELAY and int(g[0]) == 5, (col, g)
assert power[0] > coarse_peak
print("OK")
