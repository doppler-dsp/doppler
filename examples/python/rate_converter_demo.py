"""rate_converter_demo.py — RateConverter cascade demo.

Shows:
  1. Stage selection  — cascade topology printed for every rate regime
  2. Frequency check  — passband tone verified at correct output position
  3. Rate change      — set_rate() rebuilds cascade; output length changes
  4. Spectral plot    — four regimes side-by-side, each stage annotated

RateConverter chooses the cheapest cascade (CIC, HalfbandDecimator, polyphase
Resampler) at construction time and rebuilds it transparently when the rate
changes.  The four decimation regimes are:

  rate >= 1.0 or D < 2       Resampler(rate)
  D ≈ 2^1                    HalfbandDecimator
  D ≈ 2^2                    HalfbandDecimator → HalfbandDecimator
  D = 2^n, n>=3, D<=4096     CIC(D)
  D >= 8, non-power-of-2     CIC(R*) → Resampler(correction)
  otherwise                  Resampler(rate)

where D = 1/rate and R* = nearest power-of-two to D.

Run:
  python examples/python/rate_converter_demo.py
"""

from __future__ import annotations

import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.resample import RateConverter


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def _tone(freq_norm: float, n: int) -> np.ndarray:
    """Complex exponential at freq_norm (cycles/sample, −0.5…+0.5)."""
    t = np.arange(n)
    return np.exp(2j * np.pi * freq_norm * t).astype(np.complex64)


def _noise(n: int, rng: np.random.Generator) -> np.ndarray:
    re = rng.standard_normal(n).astype(np.float32)
    im = rng.standard_normal(n).astype(np.float32)
    return (re + 1j * im).astype(np.complex64) * 0.3


def _rms_db(x: np.ndarray) -> float:
    rms = float(np.sqrt(np.mean(np.abs(x) ** 2)))
    return 20.0 * math.log10(rms + 1e-300)


def _spectrum_db(
    x: np.ndarray, pad: int = 8
) -> tuple[np.ndarray, np.ndarray]:
    """Windowed FFT, returns (freq_norm, amplitude_db) over [−0.5, +0.5].

    Amplitude is normalised so a unit-amplitude complex tone reads 0 dBFS.
    Uses a Blackman-Harris window.
    """
    n = len(x)
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    w = a[0] - a[1]*np.cos(k) + a[2]*np.cos(2*k) - a[3]*np.cos(3*k)
    cg = w.mean()
    S = np.fft.fftshift(np.fft.fft(x * w, n * pad))
    amp_db = 20.0 * np.log10(np.abs(S) / (n * cg) + 1e-300)
    freq = np.linspace(-0.5, 0.5, n * pad, endpoint=False)
    return freq, amp_db


# ---------------------------------------------------------------------------
# 1. Stage selection
# ---------------------------------------------------------------------------

def demo_stage_selection():
    print("--- 1. Stage selection ---")
    rates = [2.0, 0.5, 0.25, 0.125, 0.1, 1.0/3.0]
    print(f"  {'rate':>10}  {'stages'}")
    print(f"  {'-'*10}  {'-'*40}")
    for rate in rates:
        rc = RateConverter(rate)
        labels = " → ".join(rc.stages)
        print(f"  {rate:>10.6f}  {labels}")
    print()


# ---------------------------------------------------------------------------
# 2. Frequency preservation
# ---------------------------------------------------------------------------

def demo_frequency_check():
    """Feed a passband tone and verify it appears at the expected output bin.

    A tone at input normalised frequency fn_in should appear at
    fn_out = fn_in / rate in the output (as long as fn_out < 0.5).
    We measure actual peak position and compare to the prediction.
    """
    print("--- 2. Frequency preservation ---")
    fn_in = 0.04          # well below Nyquist for all test rates
    n_in  = 4096
    x = _tone(fn_in, n_in)

    rates = [0.5, 0.25, 0.125, 0.1, 1.0/3.0]
    print(
        f"  {'rate':>8}  {'fn_expected':>12}  "
        f"{'fn_measured':>12}  {'err_bins':>10}"
    )
    print(f"  {'-'*8}  {'-'*12}  {'-'*12}  {'-'*10}")
    for rate in rates:
        rc = RateConverter(rate)
        y = np.array(rc.execute(x), copy=True)
        # drop ~5 % transient
        n_drop = max(1, len(y) // 20)
        y = y[n_drop:]

        freq, amp = _spectrum_db(y)
        peak_fn = float(freq[np.argmax(amp)])
        fn_expected = fn_in / rate
        # error in output FFT bins
        err_bins = abs(peak_fn - fn_expected) * len(y) * 8
        print(
            f"  {rate:>8.4f}  {fn_expected:>12.4f}  "
            f"{peak_fn:>12.4f}  {err_bins:>10.3f}"
        )
    print()


# ---------------------------------------------------------------------------
# 3. Rate change at runtime
# ---------------------------------------------------------------------------

def demo_rate_change():
    print("--- 3. Rate change at runtime ---")
    n_in = 1024
    x = _tone(0.05, n_in)

    rc = RateConverter(0.5)
    y1 = rc.execute(x.copy())
    print(f"  rate=0.50  stages={rc.stages}  n_in={n_in}  n_out={len(y1)}")

    rc.rate = 0.25
    y2 = rc.execute(x.copy())
    print(f"  rate=0.25  stages={rc.stages}  n_in={n_in}  n_out={len(y2)}")

    rc.rate = 2.0
    y3 = rc.execute(x.copy())
    print(f"  rate=2.00  stages={rc.stages}  n_in={n_in}  n_out={len(y3)}\n")


# ---------------------------------------------------------------------------
# 4. Spectral plot
# ---------------------------------------------------------------------------

REGIMES = [
    (0.5,         "HB (rate 0.5)"),
    (0.25,        "HB×2 (rate 0.25)"),
    (0.125,       "CIC (rate 0.125)"),
    (0.1,         "CIC+Resamp (rate 0.1)"),
]


def demo_spectral_plot(out_path="rate_converter_demo.png"):
    """Save a 5-panel spectral plot: input + four decimation regimes.

    Input: 4096-sample broadband noise + tone at fn=0.04.
    Each output panel's x-axis is normalised to the output sample rate, so
    fn_out = fn_in / rate.  Stage labels are annotated on each panel.
    Tone peaks are marked with a green arrow.
    """
    rng = np.random.default_rng(42)
    n_in  = 4096
    fn_in = 0.04
    x = _noise(n_in, rng) + _tone(fn_in, n_in)

    nrows = 1 + len(REGIMES)
    fig, axes = plt.subplots(
        nrows, 1, figsize=(10, 2.4 * nrows), constrained_layout=True
    )
    fig.suptitle(
        "RateConverter — automatic cascade selection\n"
        "Input: broadband noise + tone at fn=0.04",
        fontsize=12,
        color="#f1f5f9",
    )

    def _style(ax):
        ax.set_facecolor("#111827")
        ax.grid(True, color="#374151", lw=0.4)
        ax.tick_params(colors="#d1d5db")
        for sp in ax.spines.values():
            sp.set_color("#374151")
        ax.xaxis.label.set_color("#d1d5db")
        ax.yaxis.label.set_color("#d1d5db")
        ax.title.set_color("#f1f5f9")

    # ── input spectrum ────────────────────────────────────────────────────────
    freq_in, amp_in = _spectrum_db(x)
    axes[0].plot(freq_in, amp_in, color="#60a5fa", lw=0.8)
    axes[0].axvline(fn_in,  color="#4ade80", lw=1.2, linestyle="--")
    axes[0].axvline(-fn_in, color="#4ade80", lw=1.2, linestyle="--",
                    label=f"tone at ±{fn_in}")
    axes[0].set_xlim(-0.5, 0.5)
    axes[0].set_ylim(-90, 10)
    axes[0].set_ylabel("dBFS")
    axes[0].set_title(
        f"Input  ({n_in} samples, fn={fn_in})", loc="right", color="#f1f5f9"
    )
    axes[0].legend(
        fontsize=9, facecolor="#1f2937", edgecolor="#4b5563",
        labelcolor="#d1d5db",
    )
    _style(axes[0])

    # ── one panel per regime ──────────────────────────────────────────────────
    for ax, (rate, title) in zip(axes[1:], REGIMES):
        rc = RateConverter(rate)
        y = np.array(rc.execute(x), copy=True)
        # drop ~5 % transient
        n_drop = max(1, len(y) // 20)
        y_settled = y[n_drop:]

        freq_out, amp_out = _spectrum_db(y_settled)
        ax.plot(freq_out, amp_out, color="#60a5fa", lw=0.8)

        # Expected tone position
        fn_out = fn_in / rate
        if fn_out < 0.5:
            tone_idx = int(np.argmin(np.abs(freq_out - fn_out)))
            tone_amp = float(amp_out[tone_idx])
            ax.axvline(fn_out, color="#4ade80", lw=1.2,
                       linestyle="--", alpha=0.9)
            ax.annotate(
                f"fn={fn_out:.3f}",
                xy=(fn_out, tone_amp),
                xytext=(fn_out + 0.06, tone_amp - 15),
                color="#4ade80", fontsize=9,
                arrowprops=dict(arrowstyle="->", color="#4ade80", lw=1.0),
            )

        # Stage label in top-left corner
        stage_str = " → ".join(rc.stages)
        ax.text(
            -0.48, 5, stage_str,
            color="#fbbf24", fontsize=9, va="top",
            bbox=dict(boxstyle="round,pad=0.2",
                      facecolor="#1f2937", edgecolor="#4b5563"),
        )

        ax.set_xlim(-0.5, 0.5)
        ax.set_ylim(-90, 10)
        ax.set_ylabel("dBFS")
        ax.set_title(
            f"{title}  ({len(y_settled)} output samples)",
            loc="right", color="#f1f5f9",
        )
        _style(ax)

    axes[-1].set_xlabel("Normalised frequency (cycles/sample)")

    fig.patch.set_facecolor("#0f172a")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"--- 4. Spectral plot saved → {out_path}")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("=== doppler RateConverter cascade demo ===\n")
    demo_stage_selection()
    demo_frequency_check()
    demo_rate_change()
    demo_spectral_plot()
    print("Done.")
