"""cic_compensator_demo.py — CIC droop-compensator design and response plot.

Demonstrates doppler.resample.ciccompmf — the C implementation of the
maximally-flat CIC compensator design algorithm from:

  Molnar & Vucic, "Closed-Form Design of CIC Compensators Based on
  Maximally Flat Error Criterion," IEEE TCAS-II 58(12):926-930, 2011.
  DOI: 10.1109/TCSII.2011.2172522

Plots three panels:

  1. CIC magnitude response (at decimated output rate, 0 … fs_out/2)
  2. Compensator FIR responses for M = 5, 7, 11 taps
  3. Combined CIC × compensator response (passband shaded)

Parameters: R=16, N=4; compensator tap counts M = 5, 7, 11.

Run:
  python examples/python/cic_compensator_demo.py
"""

from __future__ import annotations

import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from doppler.resample import ciccompmf


# ---------------------------------------------------------------------------
# Frequency-response helpers
# ---------------------------------------------------------------------------

def cic_response(freqs_norm: np.ndarray, N: int, R: int,
                 M: int = 1) -> np.ndarray:
    """CIC magnitude response at the *output* (decimated) rate.

    freqs_norm is in cycles/output-sample, i.e. 0 … 0.5.

    |H(f_out)| = |sin(π f M) / (R sin(π f M / R))|^N
    """
    f = freqs_norm
    with np.errstate(divide="ignore", invalid="ignore"):
        arg_num = np.pi * f * M
        arg_den = np.pi * f * M / R
        h = np.where(
            np.abs(arg_den) < 1e-12,
            1.0,
            np.abs(np.sin(arg_num) / (R * np.sin(arg_den))) ** N,
        )
    return h


def fir_response(h: np.ndarray,
                 freqs_norm: np.ndarray) -> np.ndarray:
    """Evaluate FIR frequency response at normalised frequencies [0, 0.5]."""
    n = len(freqs_norm) * 8
    H = np.fft.rfft(h, n=n)
    f_grid = np.fft.rfftfreq(n)
    # Interpolate onto the requested grid
    H_mag = np.abs(np.interp(freqs_norm, f_grid, np.abs(H)))
    return H_mag


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    R, N_cic = 16, 4
    taps_list = [5, 7, 11]                  # compensator lengths to compare
    freqs = np.linspace(0, 0.5, 2000)       # cycles/output-sample

    cic_mag = cic_response(freqs, N_cic, R)
    cic_db  = 20.0 * np.log10(np.maximum(cic_mag, 1e-15))

    # ── figure ───────────────────────────────────────────────────────────────
    fig, axes = plt.subplots(1, 3, figsize=(13, 5), constrained_layout=True)
    fig.suptitle(
        f"CIC Compensator Design  R={R}, N={N_cic}\n"
        f"(output rate = fs_in / {R})",
        fontsize=12, color="#f1f5f9",
    )

    colours = ["#f97316", "#34d399", "#a78bfa"]

    for ax in axes:
        ax.set_facecolor("#111827")
        ax.grid(True, color="#374151", lw=0.4)
        ax.tick_params(colors="#d1d5db")
        for sp in ax.spines.values():
            sp.set_color("#374151")
        ax.xaxis.label.set_color("#d1d5db")
        ax.yaxis.label.set_color("#d1d5db")
        ax.title.set_color("#f1f5f9")

    fig.patch.set_facecolor("#0f172a")

    # Panel 1 — CIC response alone
    ax = axes[0]
    ax.plot(freqs, cic_db, color="#60a5fa", lw=1.2)
    ax.axvline(0.5 / R, color="#ffffff", lw=0.8, linestyle=":",
               label=f"Passband edge\n(f = 0.5/{R})")
    pb_edge = 0.5 / R
    droop = cic_db[np.searchsorted(freqs, pb_edge)]
    ax.annotate(
        f"Droop at\npb edge:\n{droop:.1f} dB",
        xy=(pb_edge, droop), xytext=(0.15, -8),
        color="#f97316", fontsize=9, va="top",
        arrowprops=dict(arrowstyle="->", color="#f97316", lw=1.0),
    )
    ax.set_xlim(0, 0.5)
    ax.set_ylim(-60, 2)
    ax.set_xlabel("Frequency (cycles/output-sample)")
    ax.set_ylabel("Amplitude (dB)")
    ax.set_title("CIC response", loc="right", color="#f1f5f9")
    ax.legend(loc="lower left", fontsize=9,
              facecolor="#1f2937", edgecolor="#4b5563", labelcolor="#d1d5db")

    # Panel 2 — compensator FIR responses
    ax = axes[1]
    for m, col in zip(taps_list, colours):
        h = ciccompmf(N_cic, R, m)
        comp_mag = fir_response(h, freqs)
        comp_db  = 20.0 * np.log10(np.maximum(comp_mag, 1e-15))
        ax.plot(freqs, comp_db, color=col, lw=1.2, label=f"M={m}")
    ax.axvline(0.5 / R, color="#ffffff", lw=0.8, linestyle=":")
    ax.set_xlim(0, 0.5)
    ax.set_ylim(-5, 10)
    ax.set_xlabel("Frequency (cycles/output-sample)")
    ax.set_ylabel("Amplitude (dB)")
    ax.set_title("Compensator FIR", loc="right", color="#f1f5f9")
    ax.legend(loc="upper right", fontsize=9,
              facecolor="#1f2937", edgecolor="#4b5563", labelcolor="#d1d5db")

    # Panel 3 — CIC × compensator combined
    ax = axes[2]
    ax.plot(freqs, cic_db, color="#60a5fa", lw=0.8,
            linestyle="--", label="CIC alone")
    for m, col in zip(taps_list, colours):
        h = ciccompmf(N_cic, R, m)
        comp_mag = fir_response(h, freqs)
        combined_db = 20.0 * np.log10(
            np.maximum(cic_mag * comp_mag, 1e-15))
        ax.plot(freqs, combined_db, color=col, lw=1.2, label=f"M={m}")
    pb = 0.5 / R
    ax.axvspan(0, pb, color="#1e3a5f", alpha=0.4, label="Passband")
    ax.axvline(pb, color="#ffffff", lw=0.8, linestyle=":")
    ax.axhline(0.0, color="#374151", lw=0.6, linestyle="-")
    ax.set_xlim(0, 0.5)
    ax.set_ylim(-20, 8)
    ax.set_xlabel("Frequency (cycles/output-sample)")
    ax.set_ylabel("Amplitude (dB)")
    ax.set_title("CIC × compensator", loc="right", color="#f1f5f9")
    ax.legend(loc="lower left", fontsize=9,
              facecolor="#1f2937", edgecolor="#4b5563", labelcolor="#d1d5db")

    out = "cic_compensator_demo.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved → {out}")

    # Print tap values for M=7
    h7 = ciccompmf(N_cic, R, 7)
    print(f"\nM=7 compensator taps (R={R}, N={N_cic}):")
    for i, c in enumerate(h7):
        print(f"  h[{i}] = {c:+.8f}")
    pb_droop = 20 * math.log10(
        max(cic_response(np.array([0.5 / R]), N_cic, R)[0], 1e-15))
    print(f"\nCIC passband droop at f=0.5/{R}: {pb_droop:.2f} dB")


if __name__ == "__main__":
    main()
