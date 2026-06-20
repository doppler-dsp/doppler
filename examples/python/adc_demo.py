"""adc_demo.py — ADC quantization: sinusoid at -10 dBFS, 6-10 bits.

Shows the progression from coarse (6-bit) to fine (10-bit) quantization
on a -10 dBFS test tone.  Each bit depth adds ~6 dB of SNR.

Time domain: a slow (F0=0.01) sine is used so that consecutive samples span
less than one quantization level near the peak.  The staircase is coarse and
clearly visible at 6 bits; by 10 bits the curve looks smooth.

Spectrum: one-sided Blackman-Harris amplitude spectrum of the decoded signal.
Dashed lines mark the expected per-bin spectral noise floor accounting for
the FFT processing gain (not the total noise power).  The y-axis runs to
-120 dBFS so all floors are visible.

Run:
    python examples/python/adc_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.cvt import ADC

# ── parameters ───────────────────────────────────────────────────────────────

DBFS = -10.0
BITS = [3, 4, 5, 6, 7, 8]
COLORS = ["#ef4444", "#f97316", "#eab308", "#22c55e", "#60a5fa", "#a78bfa"]
REF = "#94a3b8"

# Time domain: slow sine so ≤1 quant step between adjacent samples near peak
F0_T = 0.01  # 100 samples/cycle
N_T = 200  # two full cycles

# Spectrum: fast sine, large N for low-noise measurement floor
F0_S = 0.05  # 20 samples/cycle
N_S = 8192
PAD = 4  # zero-pad factor for spectrum

# ── helpers ──────────────────────────────────────────────────────────────────


def _blackman_harris(n: int) -> np.ndarray:
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2.0 * np.pi * np.arange(n) / n
    return (
        a[0] - a[1] * np.cos(k) + a[2] * np.cos(2 * k) - a[3] * np.cos(3 * k)
    )


def _spectrum_db(
    x: np.ndarray, pad: int = PAD
) -> tuple[np.ndarray, np.ndarray]:
    """One-sided amplitude spectrum, 0 dBFS = unit-amplitude sine."""
    n = len(x)
    w = _blackman_harris(n)
    cg = w.mean()
    S = np.abs(np.fft.rfft(x * w, n * pad))
    amp_db = 20.0 * np.log10(S / (n * cg / 2.0) + 1e-300)
    freq = np.fft.rfftfreq(n * pad)
    return freq, amp_db


def _per_bin_floor_db(adc: ADC, n: int, pad: int = PAD) -> float:
    """Expected per-bin noise floor (dBFS amplitude) in the Blackman-Harris
    spectrum, accounting for the FFT processing gain.

    White quantization noise with power Δ²/12 is spread across the rfft
    bins.  Each bin's expected amplitude (after BH windowing) is:

        floor = sqrt(noise_power / (n_fft * mean(w²) / 2))

    where n_fft = n * pad and mean(w²) is the noise power of the window.
    """
    delta = 1.0 / adc.scale
    noise_power = delta**2 / 12.0
    w = _blackman_harris(n)
    wn2 = np.mean(w**2)  # noise power of the window
    n_fft = n * pad
    per_bin_amp = np.sqrt(noise_power / (n_fft * wn2 / 2.0))
    return 20.0 * np.log10(per_bin_amp + 1e-300)


def _decode(adc: ADC, q: np.ndarray) -> np.ndarray:
    return q.astype(np.float64) / adc.scale


def _style_ax(ax: plt.Axes) -> None:
    ax.set_facecolor("#111827")
    ax.tick_params(colors="#d1d5db")
    ax.xaxis.label.set_color("#d1d5db")
    ax.yaxis.label.set_color("#d1d5db")
    ax.title.set_color("#f1f5f9")
    for sp in ax.spines.values():
        sp.set_color("#374151")
    ax.grid(True, color="#374151", lw=0.4)


# ── signals ──────────────────────────────────────────────────────────────────

amplitude = 10.0 ** (DBFS / 20.0)

# Time domain signal
t_t = np.arange(N_T, dtype=np.float64)
x_t = (amplitude * np.sin(2.0 * np.pi * F0_T * t_t)).astype(np.float32)

# Spectrum signal
t_s = np.arange(N_S, dtype=np.float64)
x_s = (amplitude * np.sin(2.0 * np.pi * F0_S * t_s)).astype(np.float32)

# ── figure ───────────────────────────────────────────────────────────────────


def main(out_path: str = "adc_demo.png") -> None:
    fig, (ax_t, ax_s) = plt.subplots(
        2, 1, figsize=(12, 10), constrained_layout=True
    )
    fig.patch.set_facecolor("#0f172a")
    fig.suptitle(
        f"ADC quantisation — {DBFS} dBFS sinusoid"
        f"  ·  {BITS[0]}–{BITS[-1]} bits",
        fontsize=13,
        color="#f1f5f9",
    )

    # ── time domain ──────────────────────────────────────────────────────────
    # Show one full cycle.  F0_T=0.01 → 100 samples/cycle so near the peak
    # (sample 25) consecutive samples differ by < one LSB for all bit depths;
    # the staircase steps are individually resolvable, getting progressively
    # finer from 6→10 bits.
    cycle_len = round(1.0 / F0_T)  # 100 samples
    t_show = min(cycle_len, N_T)

    ax_t.plot(
        x_t[:t_show],
        color=REF,
        lw=1.2,
        alpha=0.6,
        label="float32 input",
        zorder=20,
    )
    for bits, color in zip(BITS, COLORS):
        adc = ADC(bits=bits, dbfs=DBFS, dithering=0)
        x_hat = _decode(adc, adc.steps(x_t))
        snr = 6.02 * bits + 1.76
        ax_t.step(
            np.arange(t_show),
            x_hat[:t_show],
            where="mid",
            color=color,
            lw=1.1,
            alpha=0.9,
            label=f"{bits:2d} bits — SNR ≈ {snr:.0f} dB",
        )

    ax_t.set_xlim(0, t_show - 1)
    ax_t.set_xlabel("Sample")
    ax_t.set_ylabel("Amplitude")
    ax_t.set_title(
        f"1 cycle at F₀={F0_T} — staircase resolution: 2⁻ᴺ per step",
        loc="right",
    )
    ax_t.legend(
        fontsize=9,
        framealpha=0.25,
        labelcolor="#d1d5db",
        facecolor="#1e293b",
        edgecolor="#374151",
    )
    _style_ax(ax_t)

    # ── spectrum ─────────────────────────────────────────────────────────────
    freq, amp_ref = _spectrum_db(x_s.astype(np.float64))
    ax_s.plot(
        freq,
        amp_ref,
        color=REF,
        lw=0.8,
        alpha=0.5,
        label="float32 input",
    )

    for bits, color in zip(BITS, COLORS):
        adc = ADC(bits=bits, dbfs=DBFS, dithering=0)
        x_hat = _decode(adc, adc.steps(x_s))
        freq, amp = _spectrum_db(x_hat)
        snr = 6.02 * bits + 1.76
        ax_s.plot(
            freq,
            amp,
            color=color,
            lw=0.85,
            alpha=0.92,
            label=f"{bits:2d} bits — SNR ≈ {snr:.0f} dB",
        )
        # Per-bin expected noise floor (accounts for BH processing gain).
        # These lines should coincide with the visible white noise floor.
        floor_db = _per_bin_floor_db(adc, N_S, PAD)
        ax_s.axhline(
            floor_db,
            color=color,
            lw=0.5,
            ls="--",
            alpha=0.45,
        )

    ax_s.set_xlim(0.0, 0.5)
    ax_s.set_ylim(-80, 0)
    ax_s.set_xlabel("Normalised frequency (cycles/sample)")
    ax_s.set_ylabel("Amplitude (dBFS)")
    ax_s.set_title(
        f"Blackman-Harris spectrum, N={N_S}  ·  dashed = per-bin noise floor",
        loc="right",
    )
    ax_s.legend(
        fontsize=9,
        framealpha=0.25,
        labelcolor="#d1d5db",
        facecolor="#1e293b",
        edgecolor="#374151",
    )
    _style_ax(ax_s)

    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved → {out_path}")


if __name__ == "__main__":
    main()
