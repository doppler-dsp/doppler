"""corr_demo.py — doppler Corr / Corr2D / Detector / Detector2D demo.

Demonstrates all four correlation / detection classes:

  1. Corr     — coherent integrate-and-dump lifts a −6 dB signal out of noise.
                Left panel compares |R[τ]| for dwell=1 vs dwell=8.
  2. Corr2D   — 2-D template matching; correlation heatmap in centre panel.
  3. Detector — streaming push with threshold.  Right panel plots test_stat
                for alternating signal and noise-only dwell cycles.
  4. Detector2D — one-call sanity check printed to stdout.

Saves a three-panel figure to corr_demo.png.

Run:
  python examples/python/corr_demo.py
"""

import matplotlib

matplotlib.use("Agg")  # headless: no display required

import matplotlib.pyplot as plt
import numpy as np

from doppler.spectral import Corr, Corr2D, Detector, Detector2D

# ── Parameters ───────────────────────────────────────────────────────────────

N, LAG = 64, 17       # 1-D frame length and injected lag
NY, NX = 8, 8         # 2-D frame dimensions
ROW, COL = 3, 5       # 2-D shift (row, col)
SIGMA = 2.0           # noise amplitude; power = SIGMA² = 4  →  SNR ≈ −6 dB
DWELL = 8             # coherent integration depth
THRESHOLD = 5.0       # detection gate (for the bar-chart panel)

rng = np.random.default_rng(42)

# Unit-magnitude BPSK PN references — fully deterministic.
ref1d = rng.choice(np.array([-1.0, 1.0], dtype=np.float32),
                   size=N).astype(np.complex64)
ref2d = rng.choice(np.array([-1.0, 1.0], dtype=np.float32),
                   size=(NY, NX)).astype(np.complex64)


_noise_scale = np.float32(SIGMA / np.sqrt(2))


def noisy_frame() -> np.ndarray:
    """Return one N-sample CF32 frame: ref1d shifted by LAG, plus noise."""
    signal = np.roll(ref1d, LAG)
    noise = (rng.standard_normal(N) + 1j * rng.standard_normal(N)).astype(
        np.complex64
    ) * _noise_scale
    return signal + noise


def noise_block(n_frames: int) -> np.ndarray:
    """Return n_frames*N samples of CF32 noise (no signal)."""
    total = N * n_frames
    return (
        (rng.standard_normal(total) + 1j * rng.standard_normal(total)).astype(
            np.complex64
        )
        * _noise_scale
    )


print("=== doppler Corr / Corr2D / Detector / Detector2D demo ===\n")

# ── 1. Corr: dwell=1 vs dwell=8 ─────────────────────────────────────────────

with Corr(ref1d, dwell=1) as c:
    mag_d1 = np.abs(c.execute(noisy_frame()))

with Corr(ref1d, dwell=DWELL) as c:
    for _ in range(DWELL - 1):
        c.execute(noisy_frame())
    mag_d8 = np.abs(c.execute(noisy_frame()))

snr_d1 = mag_d1[LAG] / np.mean(np.delete(mag_d1, LAG))
snr_d8 = mag_d8[LAG] / np.mean(np.delete(mag_d8, LAG))
print(
    f"[Corr]      dwell=1  peak/mean={snr_d1:.1f}"
    f"  |  dwell={DWELL}  peak/mean={snr_d8:.1f}"
)

# ── 2. Corr2D: 2-D template match ───────────────────────────────────────────

x2d = np.roll(np.roll(ref2d, ROW, axis=0), COL, axis=1)
with Corr2D(ref2d, dwell=1) as c:
    surf2d = np.abs(c.execute(x2d)).reshape(NY, NX)

peak_row, peak_col = np.unravel_index(surf2d.argmax(), (NY, NX))
print(
    f"[Corr2D]    peak at (row={peak_row}, col={peak_col})"
    f"  (expected ({ROW}, {COL}))"
)

# ── 3. Detector: alternating signal / noise-only dwell cycles ────────────────
#
# threshold=0 so both signal and noise-only dumps always fire — we use the
# threshold line on the bar chart to show what would be gated in practice.

det = Detector(
    ref1d,
    dwell=DWELL,
    noise_lo=LAG + 4,
    noise_hi=N - 1,
    threshold=0.0,
)

sig_stats, noise_stats = [], []
N_CYCLES = 4
for _ in range(N_CYCLES):
    sig_block = np.concatenate([noisy_frame() for _ in range(DWELL)])
    for *_, stat in det.push(sig_block):
        sig_stats.append(stat)

    for *_, stat in det.push(noise_block(DWELL)):
        noise_stats.append(stat)

print(
    f"[Detector]  {len(sig_stats)} signal dumps  "
    f"mean stat={np.mean(sig_stats):.1f}"
    f"  |  {len(noise_stats)} noise dumps  "
    f"mean stat={np.mean(noise_stats):.1f}"
    f"  (shown threshold={THRESHOLD})"
)

# ── 4. Detector2D: one-frame sanity check ───────────────────────────────────

with Detector2D(ref2d, threshold=0) as det2d:
    hits2d = det2d.push(x2d.ravel())

if hits2d:
    r, col_hit, *_ = hits2d[0]
    print(
        f"[Detector2D] peak at (row={r}, col={col_hit})"
        f"  (expected ({ROW}, {COL}))"
    )

# ── Figure ────────────────────────────────────────────────────────────────────

fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(14, 4))

# Panel 1 — 1-D dwell comparison
tau = np.arange(N)
ax1.plot(tau, mag_d1, lw=1, color="tab:blue",
         label=f"dwell=1  (peak/mean={snr_d1:.1f})")
ax1.plot(tau, mag_d8, lw=1.5, color="tab:orange",
         label=f"dwell={DWELL}  (peak/mean={snr_d8:.1f})")
ax1.axvline(LAG, color="0.5", ls="--", lw=0.8, label=f"lag={LAG}")
ax1.set_xlabel("lag τ")
ax1.set_ylabel("|R[τ]|")
ax1.set_title(f"1-D Corr: dwell comparison (SNR ≈ −6 dB)")
ax1.legend(fontsize=8)
ax1.grid(alpha=0.3)

# Panel 2 — Corr2D heatmap
im = ax2.imshow(surf2d, origin="upper", cmap="viridis", aspect="equal")
ax2.plot(COL, ROW, "r*", ms=14, label=f"peak ({ROW},{COL})")
ax2.set_xlabel("col offset")
ax2.set_ylabel("row offset")
ax2.set_title("Corr2D: correlation surface")
ax2.legend(fontsize=8, loc="lower right")
fig.colorbar(im, ax=ax2, fraction=0.046)

# Panel 3 — Detector test_stat per dump
n = N_CYCLES
xs = np.arange(n)
w = 0.35
ax3.bar(xs - w / 2, sig_stats,   w, color="tab:blue", label="signal")
ax3.bar(xs + w / 2, noise_stats, w, color="tab:gray", label="noise-only")
ax3.axhline(THRESHOLD, color="tab:red", ls="--", lw=1,
            label=f"threshold={THRESHOLD}")
ax3.set_xticks(xs)
ax3.set_xticklabels([f"cycle {i+1}" for i in xs])
ax3.set_ylabel("test_stat  (peak / noise_est)")
ax3.set_title(f"Detector: test_stat per dwell cycle (dwell={DWELL})")
ax3.legend(fontsize=8)
ax3.grid(alpha=0.3, axis="y")

fig.tight_layout()
out_path = "corr_demo.png"
fig.savefig(out_path, dpi=120)
print(f"\nwrote {out_path}")
