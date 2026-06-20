"""cvt_quantization_demo.py — Quantization noise comparison for cvt converters.

Each cvt converter operates on real F32 samples.  For complex signals
(float _Complex / CF32 — two 32-bit floats per sample) we view the array
as interleaved F32 [re₀, im₀, re₁, im₁, …], apply the converter to all
values, then view the output back as CF32.

Converters compared:
  - F32ToI16   / I16ToF32      — signed int16 (bipolar, two's-complement)
  - F32ToI16U32 / I16U32ToF32  — uint32 container (same Q15 bit pattern,
                                  zero-extended)
  - F32ToI16U64 / I16U64ToF32  — uint64 container (same Q15 bit pattern,
                                  zero-extended)

All three share the same Q15 quantization step (1/32768 ≈ 3.05e-5), so
their quantization noise floors are identical.  The container type determines
the downstream integer word width, not the quantization error.

Output: cvt_quantization_demo.png

Run:
  python examples/python/cvt_quantization_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

import doppler.cvt as cvt

# ---------------------------------------------------------------------------
# converters
# ---------------------------------------------------------------------------

CONVERTERS = [
    ("F32ToI16", "I16ToF32", "#60a5fa", "F32→I16→F32   (int16)"),
    ("F32ToI16U32", "I16U32ToF32", "#f97316", "F32→I16U32→F32 (uint32)"),
    ("F32ToI16U64", "I16U64ToF32", "#a78bfa", "F32→I16U64→F32 (uint64)"),
]


def _roundtrip_cf32(x: np.ndarray, fwd_name: str, inv_name: str) -> np.ndarray:
    """Quantize a CF32 array through a real F32 converter.

    The real and imaginary channels of each float _Complex sample are two
    independent F32 values.  Each channel is converted separately so the
    converter never sees interleaved data.
    """
    fwd = getattr(cvt, fwd_name)
    inv = getattr(cvt, inv_name)

    re = np.ascontiguousarray(x.real)  # float32, shape (N,)
    im = np.ascontiguousarray(x.imag)  # float32, shape (N,)

    re_q = inv().steps(fwd().steps(re))
    im_q = inv().steps(fwd().steps(im))

    return (re_q + 1j * im_q).astype(np.complex64)


# ---------------------------------------------------------------------------
# signal
# ---------------------------------------------------------------------------


def _tone(freq_norm: float, n: int) -> np.ndarray:
    """Complex exponential at freq_norm (cycles/sample)."""
    t = np.arange(n, dtype=np.float64)
    return np.exp(2j * np.pi * freq_norm * t).astype(np.complex64)


def _make_signal(n: int) -> np.ndarray:
    """Multi-tone CF32 signal spanning a wide dynamic range.

    Complex tones at 0 dBFS, −20 dBFS, −40 dBFS, −60 dBFS, −80 dBFS
    at distinct normalised frequencies.
    """
    # Scale so the worst-case coherent sum of all peaks stays below Q15
    # full-scale (32767/32768 ≈ 0.99997).  Sum of peak amplitudes ≈ 0.889,
    # leaving >10% headroom even when all cosines align simultaneously.
    tones = [
        (0.07, 0.80),  #   0 dBFS (dominant)
        (0.13, 0.080),  # −20 dBFS
        (0.21, 0.0080),  # −40 dBFS
        (
            -0.31,
            0.00080,
        ),  # −60 dBFS  (negative freq — only visible in full spectrum)
        (0.43, 0.000080),  # −80 dBFS
    ]
    return sum(a * _tone(f, n) for f, a in tones).astype(np.complex64)


# ---------------------------------------------------------------------------
# spectrum
# ---------------------------------------------------------------------------


def _blackman_harris(n: int) -> np.ndarray:
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    return (
        a[0] - a[1] * np.cos(k) + a[2] * np.cos(2 * k) - a[3] * np.cos(3 * k)
    )


def _spectrum_db(x: np.ndarray, pad: int = 4) -> tuple[np.ndarray, np.ndarray]:
    """Full complex spectrum (−0.5…+0.5), Blackman-Harris windowed, normalised.
    """
    n = len(x)
    w = _blackman_harris(n)
    cg = w.mean()
    S = np.fft.fftshift(np.fft.fft(x * w, n * pad))
    amp_db = 20.0 * np.log10(np.abs(S) / (n * cg) + 1e-300)
    freq = np.fft.fftshift(np.fft.fftfreq(n * pad))
    return freq, amp_db


# ---------------------------------------------------------------------------
# plot
# ---------------------------------------------------------------------------


def _style_ax(ax):
    ax.set_facecolor("#111827")
    ax.tick_params(colors="#d1d5db")
    ax.xaxis.label.set_color("#d1d5db")
    ax.yaxis.label.set_color("#d1d5db")
    ax.title.set_color("#f1f5f9")
    for sp in ax.spines.values():
        sp.set_color("#374151")
    ax.grid(True, color="#374151", lw=0.4)


def main(out_path: str = "cvt_quantization_demo.png") -> None:
    n = 65536
    x = _make_signal(n)

    freq, amp_in = _spectrum_db(x)

    roundtrips = [
        (label, color, _roundtrip_cf32(x, fwd, inv))
        for fwd, inv, color, label in CONVERTERS
    ]

    fig, (ax_in, ax_q, ax_err) = plt.subplots(
        3, 1, figsize=(10, 10), constrained_layout=True
    )
    fig.patch.set_facecolor("#0f172a")
    fig.suptitle(
        "cvt quantization — CF32 input"
        " (two F32 per sample, interleaved re/im)\n"
        "Q15 step = 1/32768 ≈ 3.05e-5  (all three converters identical)",
        fontsize=12,
        color="#f1f5f9",
    )

    # ── input spectrum ───────────────────────────────────────────────────────
    ax_in.plot(freq, amp_in, color="#94a3b8", lw=0.9, label="Input CF32")
    ax_in.set_xlim(-0.5, 0.5)
    ax_in.set_ylim(-130, 10)
    ax_in.set_xlabel("Normalised frequency (cycles/sample)")
    ax_in.set_ylabel("Amplitude (dBFS)")
    ax_in.set_title("Input signal spectrum (complex)", loc="right")
    ax_in.legend(
        facecolor="#1f2937", edgecolor="#4b5563", labelcolor="#d1d5db"
    )
    _style_ax(ax_in)

    # ── quantised output spectra (overlaid) ──────────────────────────────────
    ax_q.plot(
        freq, amp_in, color="#94a3b8", lw=0.8, alpha=0.4, label="Input CF32"
    )
    for label, color, xq in roundtrips:
        _, amp_q = _spectrum_db(xq)
        ax_q.plot(freq, amp_q, color=color, lw=0.9, alpha=0.85, label=label)
    ax_q.set_xlim(-0.5, 0.5)
    ax_q.set_ylim(-130, 10)
    ax_q.set_xlabel("Normalised frequency (cycles/sample)")
    ax_q.set_ylabel("Amplitude (dBFS)")
    ax_q.set_title("Quantised output spectra", loc="right")
    ax_q.legend(
        facecolor="#1f2937",
        edgecolor="#4b5563",
        labelcolor="#d1d5db",
        fontsize=10,
    )
    _style_ax(ax_q)

    # ── error spectra ────────────────────────────────────────────────────────
    for label, color, xq in roundtrips:
        err = xq - x
        _, amp_err = _spectrum_db(err)
        ax_err.plot(
            freq, amp_err, color=color, lw=0.9, alpha=0.85, label=label
        )
    ax_err.set_xlim(-0.5, 0.5)
    ax_err.set_ylim(-130, -70)
    ax_err.set_xlabel("Normalised frequency (cycles/sample)")
    ax_err.set_ylabel("Error amplitude (dBFS)")
    ax_err.set_title("Quantisation error spectrum", loc="right")
    ax_err.legend(
        facecolor="#1f2937",
        edgecolor="#4b5563",
        labelcolor="#d1d5db",
        fontsize=10,
    )
    _style_ax(ax_err)

    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved → {out_path}")


if __name__ == "__main__":
    main()
