"""awgn_demo.py — AWGN generator demo.

Shows:
  1. Basic generation    — complex CF32 noise, amplitude = per-component std dev
  2. Statistics          — empirical mean ≈ 0, std dev ≈ amplitude, Gaussian shape
  3. White spectrum      — PSD flat to within thermal noise floor
  4. Seeding             — deterministic replay via reset() / reseed()
  5. Spectral plot       — histogram + Welch PSD + noisy carrier saved to
                           docs/assets/awgn_demo.png

Run:
  python examples/python/awgn_demo.py
"""

from __future__ import annotations

import math
import pathlib
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.signal import welch

from doppler.source import AWGN, LO


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def _rms_db(x: np.ndarray) -> float:
    return 20.0 * math.log10(float(np.sqrt(np.mean(np.abs(x) ** 2))) + 1e-300)


# ---------------------------------------------------------------------------
# 1. Basic generation
# ---------------------------------------------------------------------------

g = AWGN(seed=42, amplitude=1.0)
noise = g.generate(65536)
assert noise.dtype == np.complex64
assert noise.shape == (65536,)
re = np.real(noise).astype(np.float64)
im = np.imag(noise).astype(np.float64)
print(f"  Re: mean={re.mean():.4f}  std={re.std():.4f}")
print(f"  Im: mean={im.mean():.4f}  std={im.std():.4f}")
assert abs(re.mean()) < 0.02, f"non-zero mean: {re.mean()}"
assert abs(im.mean()) < 0.02, f"non-zero mean: {im.mean()}"
assert abs(re.std() - 1.0) < 0.02, f"std dev off: {re.std()}"
assert abs(im.std() - 1.0) < 0.02, f"std dev off: {im.std()}"

# ---------------------------------------------------------------------------
# 2. Amplitude control
# ---------------------------------------------------------------------------

g05 = AWGN(seed=0, amplitude=0.5)
n05 = g05.generate(65536)
std05 = float(np.std(np.real(n05).astype(np.float64)))
assert abs(std05 - 0.5) < 0.02, f"amplitude=0.5 → std {std05}"

g2 = AWGN(seed=0, amplitude=2.0)
n2 = g2.generate(65536)
std2 = float(np.std(np.real(n2).astype(np.float64)))
assert abs(std2 - 2.0) < 0.05, f"amplitude=2.0 → std {std2}"

# ---------------------------------------------------------------------------
# 3. Seeding / reset reproducibility
# ---------------------------------------------------------------------------

ga = AWGN(seed=7, amplitude=1.0)
run1 = ga.generate(1024)
ga.reset()
run2 = ga.generate(1024)
assert np.array_equal(run1, run2), "reset() did not reproduce the same stream"

gb = AWGN(seed=7, amplitude=1.0)
run3 = gb.generate(1024)
assert np.array_equal(run1, run3), "same seed → same stream"

gc = AWGN(seed=7, amplitude=1.0)
gc.reseed(99)
run_diff = gc.generate(1024)
assert not np.array_equal(run1, run_diff), "reseed(99) must differ from seed 7"

# ---------------------------------------------------------------------------
# 4. Spectral flatness via Welch PSD
# ---------------------------------------------------------------------------

g_flat = AWGN(seed=0, amplitude=1.0)
x_flat = g_flat.generate(65536)
fs_ref = 1.0
freqs, Pxx = welch(
    np.real(x_flat).astype(np.float64),
    fs=fs_ref, nperseg=1024, noverlap=512
)
Pxx_db = 10.0 * np.log10(Pxx + 1e-300)
# Use 5th–95th percentile to ignore the few noisiest Welch bins at band edges.
p5, p95 = float(np.percentile(Pxx_db, 5)), float(np.percentile(Pxx_db, 95))
span = p95 - p5
print(f"  Welch PSD 5th–95th percentile span: {span:.1f} dB")
assert span < 4.0, f"PSD not flat enough: {span:.1f} dB (p5→p95)"

# ---------------------------------------------------------------------------
# 5. Spectral plot (three panels)
# ---------------------------------------------------------------------------

N_PLOT = 65536
AMP    = 1.0
SEED   = 42

g_plot = AWGN(seed=SEED, amplitude=AMP)
noise_plot = g_plot.generate(N_PLOT)

# Panel 1 — Gaussian histogram (Re and Im components)
re_plot = np.real(noise_plot).astype(np.float64)
im_plot = np.imag(noise_plot).astype(np.float64)
bins = np.linspace(-4.0, 4.0, 80)
x_pdf = np.linspace(-4.5, 4.5, 500)
sigma = AMP
y_pdf = np.exp(-0.5 * (x_pdf / sigma) ** 2) / (sigma * math.sqrt(2 * math.pi))

# Panel 2 — Welch PSD (both components → total noise floor)
freqs_re, Pxx_re = welch(re_plot, fs=1.0, nperseg=1024, noverlap=512)
freqs_im, Pxx_im = welch(im_plot, fs=1.0, nperseg=1024, noverlap=512)
Pxx_total = Pxx_re + Pxx_im          # complex power = Re²+Im²
Pxx_db_plot = 10.0 * np.log10(Pxx_total + 1e-300)
expected_floor = 10.0 * math.log10(2.0 * AMP**2)   # total complex power ÷ bandwidth

# Panel 3 — Noisy carrier: LO(0.1) + AWGN(0.3) — first 256 samples (real part)
N_CARRIER = 256
lo = LO(0.1)
carrier = lo.steps(N_CARRIER)
g_noise = AWGN(seed=0, amplitude=0.3)
noise_carrier = g_noise.generate(N_CARRIER)
noisy = carrier + noise_carrier
t_axis = np.arange(N_CARRIER)

# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------

fig, (ax_hist, ax_psd, ax_sig) = plt.subplots(
    3, 1, figsize=(10, 10), constrained_layout=True
)
fig.patch.set_facecolor("#0f172a")

PANEL_BG   = "#111827"
GRID_COL   = "#374151"
LABEL_COL  = "#d1d5db"
TITLE_COL  = "#f1f5f9"
RE_COL     = "#60a5fa"    # blue — Re
IM_COL     = "#a78bfa"    # violet — Im
PDF_COL    = "#f59e0b"    # amber — theoretical PDF
PSD_COL    = "#34d399"    # green — PSD trace
FLOOR_COL  = "#f87171"    # red — expected floor
SIG_COL    = "#818cf8"    # indigo — noisy signal
TON_COL    = "#4ade80"    # green — clean carrier

# --- Histogram ---
ax_hist.set_facecolor(PANEL_BG)
ax_hist.set_title("Amplitude distribution — Re / Im vs. theoretical Gaussian",
                  color=TITLE_COL, fontsize=11)
ax_hist.hist(re_plot, bins=bins, density=True, alpha=0.6,
             color=RE_COL,  label="Re")
ax_hist.hist(im_plot, bins=bins, density=True, alpha=0.6,
             color=IM_COL,  label="Im")
ax_hist.plot(x_pdf, y_pdf, color=PDF_COL, lw=2.0,
             label=f"N(0, σ²={AMP}²)")
ax_hist.set_xlabel("Amplitude", color=LABEL_COL)
ax_hist.set_ylabel("Probability density", color=LABEL_COL)
ax_hist.legend(framealpha=0.3, labelcolor="white")
ax_hist.grid(color=GRID_COL, lw=0.5)
ax_hist.tick_params(colors=LABEL_COL)

# --- Welch PSD ---
ax_psd.set_facecolor(PANEL_BG)
ax_psd.set_title(
    "Power spectral density — Welch (Re²+Im²), N=65536, nperseg=1024",
    color=TITLE_COL, fontsize=11)
ax_psd.plot(freqs_re, Pxx_db_plot, color=PSD_COL, lw=1.0, label="Measured PSD")
ax_psd.axhline(expected_floor, color=FLOOR_COL, lw=1.5, linestyle="--",
               label=f"Expected floor = 2σ²/BW ≈ {expected_floor:.1f} dB")
ax_psd.set_xlabel("Normalised frequency (cycles/sample)", color=LABEL_COL)
ax_psd.set_ylabel("Power (dB)", color=LABEL_COL)
ax_psd.legend(framealpha=0.3, labelcolor="white")
ax_psd.grid(color=GRID_COL, lw=0.5)
ax_psd.tick_params(colors=LABEL_COL)
p2p = float(Pxx_db_plot.max() - Pxx_db_plot.min())
ax_psd.set_title(
    f"Power spectral density — Welch (Re²+Im²)  p-p={p2p:.1f} dB",
    color=TITLE_COL, fontsize=11)

# --- Noisy carrier ---
ax_sig.set_facecolor(PANEL_BG)
ax_sig.set_title(
    "Noisy carrier — LO(0.1) + AWGN(σ=0.3), real part, SNR ≈ "
    f"{_rms_db(carrier) - _rms_db(noise_carrier[:N_CARRIER]):.1f} dB",
    color=TITLE_COL, fontsize=11)
ax_sig.plot(t_axis, np.real(noisy).astype(np.float32),
            color=SIG_COL, lw=0.8, alpha=0.9, label="Re{LO + AWGN}")
ax_sig.plot(t_axis, np.real(carrier).astype(np.float32),
            color=TON_COL, lw=1.2, linestyle="--", alpha=0.7, label="Re{LO} (clean)")
ax_sig.set_xlabel("Sample index", color=LABEL_COL)
ax_sig.set_ylabel("Amplitude", color=LABEL_COL)
ax_sig.legend(framealpha=0.3, labelcolor="white")
ax_sig.grid(color=GRID_COL, lw=0.5)
ax_sig.tick_params(colors=LABEL_COL)

for ax in (ax_hist, ax_psd, ax_sig):
    ax.xaxis.label.set_color(LABEL_COL)
    ax.yaxis.label.set_color(LABEL_COL)
    for spine in ax.spines.values():
        spine.set_edgecolor(GRID_COL)
    ax.tick_params(colors=LABEL_COL)

out_path = pathlib.Path("docs/assets/awgn_demo.png")
fig.savefig(out_path, dpi=150, bbox_inches="tight")
plt.close(fig)
print(f"  Saved {out_path}")

print("awgn_demo: OK")
