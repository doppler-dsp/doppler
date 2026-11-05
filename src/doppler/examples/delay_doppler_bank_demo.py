"""delay_doppler_bank_demo.py — find a code on an unknown carrier.

A code delayed by 37 samples arrives on a carrier 1.3 code bins off
nominal, in noise. A delay-Doppler map corrects no carrier within a code
period, so a carrier d bins off costs the Dirichlet factor of d: 3.9 dB at
half a bin, 19 dB at 0.9 bin, a null at a whole bin. The bank searches the
carrier, and loses only the factor of the residual to its nearest grid
carrier: ``bank_detect`` makes one map per carrier on a coarse grid (half
bins), picks the candidates, refines each at its own cell on a fine grid
(quarter bins), and keeps what clears the confirm threshold. Every carrier
shares the frame's row transforms, so the coarse search costs half the fine
one, and the refine recovers what the coarse grid's straddle lost
(``docs/design/corr2d-hypotheses.md`` section 6).

The thresholds come from noise-only frames, as a detector's must. The C twin
is ``native/examples/delay_doppler_bank_demo.c``.

Saves a three-panel figure to delay_doppler_bank_demo.png: the delay-Doppler
map at the best coarse carrier, the strongest cell's power against carrier
(where the coarse grid's half-bin steps fall short of the true carrier and
the refined quarter-bin step lands near it), and the straddle loss of each.

Run:
  python delay_doppler_bank_demo.py
"""

from doppler.examples._extra import require

require("matplotlib")
import matplotlib

matplotlib.use("Agg")  # headless: no display required

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

# --8<-- [start:curve]
# What the straddle looks like: the strongest cell's power under each carrier
# of an eighth-bin grid (-2 to +3 bins), one single-carrier bank apiece.
fine = np.arange(-16, 25)
curve = np.array(
    [
        float(dd.bank_power_max(x, 8, int(g), int(g), NO_RATES)[0].max())
        for g in fine
    ]
)
best = float(curve.max())
loss_coarse = 10 * np.log10(coarse_peak / best)  # dB below the best carrier
loss_refined = 10 * np.log10(float(power[0]) / best)
print(
    f"best carrier on the eighth-bin grid: {fine[np.argmax(curve)] / 8:+.3f} "
    f"bins; coarse grid {loss_coarse:+.2f} dB, refined {loss_refined:+.2f} dB"
)
# --8<-- [end:curve]

# The coarse grid sits below the best carrier (the straddle: the true 1.3 bins
# falls between its 1.0 and 1.5), the refine recovers most of it, and the
# best carrier is the true one.
assert loss_coarse < -0.2, loss_coarse
assert loss_refined > loss_coarse + 0.2 and loss_refined > -0.25
assert abs(fine[np.argmax(curve)] / 8 - CARRIER_BINS) <= 0.25
print("OK")

import matplotlib.pyplot as plt

coarse_g = np.arange(*COARSE)  # the coarse carriers g, on the 1/(2 NX) grid
coarse_pk = np.array(
    [
        float(dd.bank_power_max(x, 2, int(g), int(g), NO_RATES)[0].max())
        for g in coarse_g
    ]
)
g_coarse = int(coarse_g[np.argmax(coarse_pk)])
cmap = dd.bank_power_max(x, 2, g_coarse, g_coarse, NO_RATES)[0].reshape(NY, NX)

fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(15, 4.2))

# Panel 1 -- the map at the best coarse carrier
im = ax1.imshow(
    10 * np.log10(cmap / cmap.max() + 1e-6),
    origin="lower",
    aspect="auto",
    extent=(-0.5, NX - 0.5, -NY // 2 - 0.5, NY - NY // 2 - 0.5),
    cmap="viridis",
    vmin=-30,
)
ax1.plot(
    DELAY,
    int(row[0]) - NY // 2,
    "o",
    mfc="none",
    mec="r",
    ms=16,
    mew=1.5,
    label="detected cell",
)
ax1.set_xlabel("delay (samples)")
ax1.set_ylabel("Doppler row (0 = zero Doppler)")
ax1.set_title(f"map at coarse carrier {g_coarse / 2:+.1f} bins")
ax1.legend(fontsize=8, loc="upper right")
fig.colorbar(im, ax=ax1, fraction=0.046, label="dB re peak")

# Panel 2 -- the straddle
ax2.plot(
    fine / 8,
    10 * np.log10(curve / best),
    color="0.4",
    lw=1.5,
    label="eighth-bin grid",
)
ax2.plot(
    coarse_g / 2,
    10 * np.log10(coarse_pk / best),
    "o",
    color="tab:orange",
    label="coarse grid (half bins)",
)
ax2.plot(
    g[0] / 4,
    loss_refined,
    "s",
    color="tab:green",
    ms=9,
    label="refined (quarter bins)",
)
ax2.axvline(
    CARRIER_BINS,
    color="tab:red",
    ls="--",
    lw=1,
    label=f"true carrier {CARRIER_BINS}",
)
ax2.set_xlabel("carrier (code bins)")
ax2.set_ylabel("strongest cell, dB re best")
ax2.set_title("the straddle, and the refine")
ax2.set_ylim(-12, 1.5)
ax2.legend(fontsize=8, loc="lower left")
ax2.grid(alpha=0.3)

# Panel 3 -- what each costs
ax3.bar(
    ["coarse\n(half bins)", "refined\n(quarter bins)"],
    [loss_coarse, loss_refined],
    color=["tab:orange", "tab:green"],
)
ax3.axhline(0.0, color="0.3", lw=0.8)
ax3.set_ylabel("loss vs the best carrier (dB)")
ax3.set_title("loss recovered by the refine")
for i, v in enumerate([loss_coarse, loss_refined]):
    ax3.text(i, v - 0.12, f"{v:+.2f} dB", ha="center", va="top", fontsize=9)
ax3.set_ylim(min(loss_coarse, -1) - 0.8, 0.4)
ax3.grid(alpha=0.3, axis="y")

fig.tight_layout()
out_path = "delay_doppler_bank_demo.png"
fig.savefig(out_path, dpi=120)
print(f"wrote {out_path}")
