"""adc_demo.py — ADC quantization: sinusoid at -10 dBFS, 6-10 bits.

Shows the progression from coarse (6-bit) to fine (10-bit) quantization
on the same -10 dBFS test tone.  Each bit depth adds ~6 dB of SNR, which
appears as a 6 dB drop in the wideband noise floor and a visibly smoother
staircase in the time domain.

Time domain: 3 cycles of the decoded sinusoid — staircase is clearly visible
at 6 bits, nearly invisible at 10 bits.

Spectrum: one-sided Blackman-Harris spectrum of the decoded signal.  The
quantisation noise floor descends at 6.02 dB per bit.  The tone stays fixed
at -10 dBFS; the SNR improves as the noise floor drops away from it.

Run:
    python examples/python/adc_demo.py
"""

from __future__ import annotations

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.cvt import ADC

# ── parameters ────────────────────────────────────────────────────────────────

DBFS   = -10.0            # input level
N      = 8192             # signal length
F0     = 0.05             # normalised frequency (20 samples/cycle)
BITS   = [6, 7, 8, 9, 10]
COLORS = ["#ef4444", "#f97316", "#eab308", "#22c55e", "#60a5fa"]
REF    = "#94a3b8"        # float32 reference trace

# ── helpers ───────────────────────────────────────────────────────────────────

def _blackman_harris(n: int) -> np.ndarray:
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2.0 * np.pi * np.arange(n) / n
    return a[0] - a[1]*np.cos(k) + a[2]*np.cos(2*k) - a[3]*np.cos(3*k)


def _spectrum_db(
    x: np.ndarray, pad: int = 4
) -> tuple[np.ndarray, np.ndarray]:
    """One-sided amplitude spectrum calibrated so a full-scale sine = 0 dBFS."""
    n  = len(x)
    w  = _blackman_harris(n)
    cg = w.mean()
    S  = np.abs(np.fft.rfft(x * w, n * pad))
    # Normalise so |S[tone_bin]| / norm == amplitude of the input sine.
    # For rfft the tone splits between +f and (implicitly) -f; dividing by
    # n*cg/2 recovers the single-sided amplitude directly.
    amp_db = 20.0 * np.log10(S / (n * cg / 2.0) + 1e-300)
    freq   = np.fft.rfftfreq(n * pad)
    return freq, amp_db


def _style_ax(ax: plt.Axes) -> None:
    ax.set_facecolor("#111827")
    ax.tick_params(colors="#d1d5db")
    ax.xaxis.label.set_color("#d1d5db")
    ax.yaxis.label.set_color("#d1d5db")
    ax.title.set_color("#f1f5f9")
    for sp in ax.spines.values():
        sp.set_color("#374151")
    ax.grid(True, color="#374151", lw=0.4)


def _decode(adc: ADC, q: np.ndarray) -> np.ndarray:
    """Reconstruct float samples from int64 ADC output."""
    return q.astype(np.float64) / adc.scale

# ── signal ────────────────────────────────────────────────────────────────────

amplitude = 10.0 ** (DBFS / 20.0)
t = np.arange(N, dtype=np.float64)
x = (amplitude * np.sin(2.0 * np.pi * F0 * t)).astype(np.float32)

# ── figure ────────────────────────────────────────────────────────────────────

def main(out_path: str = "adc_demo.png") -> None:
    cycles_shown = 3
    t_show = int(cycles_shown / F0)   # samples to display in time panel

    fig, (ax_t, ax_s) = plt.subplots(
        2, 1, figsize=(12, 10), constrained_layout=True
    )
    fig.patch.set_facecolor("#0f172a")
    fig.suptitle(
        f"ADC quantisation — {DBFS} dBFS sinusoid  ·  {BITS[0]}–{BITS[-1]} bits",
        fontsize=13, color="#f1f5f9",
    )

    # ── time domain ───────────────────────────────────────────────────────────
    ax_t.plot(
        x[:t_show], color=REF, lw=1.4, label="float32 input", zorder=20,
    )
    for bits, color in zip(BITS, COLORS):
        adc   = ADC(bits=bits, dbfs=DBFS, dithering=0)
        x_hat = _decode(adc, adc.steps(x))
        snr   = 6.02 * bits + 1.76
        ax_t.step(
            np.arange(t_show), x_hat[:t_show], where="mid",
            color=color, lw=1.1, alpha=0.9,
            label=f"{bits:2d} bits — SNR ≈ {snr:.0f} dB",
        )

    ax_t.set_xlim(0, t_show - 1)
    ax_t.set_xlabel("Sample")
    ax_t.set_ylabel("Amplitude")
    ax_t.set_title(f"{cycles_shown} cycles", loc="right")
    ax_t.legend(
        fontsize=9, framealpha=0.25, labelcolor="#d1d5db",
        facecolor="#1e293b", edgecolor="#374151",
    )
    _style_ax(ax_t)

    # ── spectrum ──────────────────────────────────────────────────────────────
    freq, amp_ref = _spectrum_db(x.astype(np.float64))
    ax_s.plot(freq, amp_ref, color=REF, lw=0.8, alpha=0.5, label="float32 input")

    for bits, color in zip(BITS, COLORS):
        adc   = ADC(bits=bits, dbfs=DBFS, dithering=0)
        x_hat = _decode(adc, adc.steps(x))
        freq, amp = _spectrum_db(x_hat)
        snr = 6.02 * bits + 1.76
        ax_s.plot(
            freq, amp, color=color, lw=0.85, alpha=0.92,
            label=f"{bits:2d} bits — SNR ≈ {snr:.0f} dB",
        )
        # Theoretical noise floor: -(6.02N + 1.76) dBFS
        floor_db = -(6.02 * bits + 1.76)
        ax_s.axhline(
            floor_db, color=color, lw=0.5, ls="--", alpha=0.35,
        )

    ax_s.set_xlim(0.0, 0.5)
    ax_s.set_ylim(-90, 5)
    ax_s.set_xlabel("Normalised frequency (cycles/sample)")
    ax_s.set_ylabel("Amplitude (dBFS)")
    ax_s.set_title("Blackman-Harris spectrum, N=8192", loc="right")
    ax_s.legend(
        fontsize=9, framealpha=0.25, labelcolor="#d1d5db",
        facecolor="#1e293b", edgecolor="#374151",
    )
    _style_ax(ax_s)

    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved → {out_path}")


if __name__ == "__main__":
    main()
