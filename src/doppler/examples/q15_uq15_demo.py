"""q15_uq15_demo.py — Q15 (bipolar) vs UQ15 (offset-binary) comparison.

Q15  (bipolar):       0.0 → int16   0      (two's-complement, sign-extended)
UQ15 (offset-binary): 0.0 → uint16 32768   (all values shifted +32768)

Both use the same quantization step Δ = 2⁻¹⁵ so the noise floor is
identical.  The difference is a DC offset in the integer domain that
cancels exactly when decoded with the matching convention.

This demo shows:
  1. Input CF32 spectrum.
  2. Q15 bipolar roundtrip spectrum — nearly identical to input; ~92 dB SNR.
  3. UQ15 offset-binary roundtrip spectrum — identical noise floor to Q15.

Run:
  python examples/python/q15_uq15_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# --8<-- [start:roundtrips]
import numpy as np

from doppler.cvt import F32ToI16, I16ToF32

# full-scale complex tone at 0.07 cycles/sample (the demo's signal)
x = np.exp(2j * np.pi * 0.07 * np.arange(65536)).astype(np.complex64)

# Q15 bipolar roundtrip
enc, dec = F32ToI16(), I16ToF32()
x_q15 = dec.steps(enc.steps(x.real))  # real channel only

# UQ15 offset-binary roundtrip (numpy — no cvt UQ15 type yet)
v = np.clip(np.round(x.real * 32768.0), -32768, 32767).astype(np.int16)
u = (v.astype(np.int32) + 32768).astype(np.uint16)
x_uq15 = (u.astype(np.float32) - 32768.0) / 32768.0
# --8<-- [end:roundtrips]

# ---------------------------------------------------------------------------
# quantizers
# ---------------------------------------------------------------------------


def _q15_roundtrip(x: np.ndarray) -> np.ndarray:
    """Q15 bipolar roundtrip via cvt.F32ToI16 / I16ToF32."""
    return I16ToF32().steps(F32ToI16().steps(x))


def _uq15_roundtrip(x: np.ndarray) -> np.ndarray:
    """UQ15 offset-binary roundtrip (correct encode + correct decode).

    Encode: v_q15 = round(x * 32768); u = v_q15 + 32768  → [0, 65535]
    Decode: x̂ = (u - 32768) / 32768

    The +32768 shift cancels exactly on decode; quantisation error is
    identical to the Q15 path.
    """
    v = np.clip(np.round(x * 32768.0), -32768, 32767).astype(np.int16)
    u = (v.astype(np.int32) + 32768).astype(np.uint16)
    return (u.astype(np.float32) - 32768.0) / 32768.0


def _uq15_wrongdecode(x: np.ndarray) -> np.ndarray:
    """UQ15 encoded, then decoded as if it were Q15 (offset never removed).

    The uint16 value u = v_q15 + 32768 is reinterpreted as a signed int16
    and divided by 32768.  Because bit 15 of u is the sign bit of the
    Q15 value (not the sign of the offset-binary value), the mapping is:

      v_q15 >= 0  →  u ∈ [32768, 65535]  →  int16 ∈ [-32768, -1]
                  →  wrong output ≈  x - 1          (sign flip + bias)

      v_q15 <  0  →  u ∈ [0, 32767]      →  int16 ∈ [0, 32767]
                  →  wrong output ≈  x + 1          (bias only)

    Result: a ±1.0 sign-dependent bias appears on every sample, producing
    large harmonic distortion at odd multiples of any input tone.
    """
    v = np.clip(np.round(x * 32768.0), -32768, 32767).astype(np.int16)
    u = (v.astype(np.int32) + 32768).astype(np.uint16)
    return u.view(np.int16).astype(np.float32) / 32768.0


def _cf32_apply(fn, x: np.ndarray) -> np.ndarray:
    """Apply a real quantiser independently to re and im channels."""
    re = np.ascontiguousarray(x.real)
    im = np.ascontiguousarray(x.imag)
    return (fn(re) + 1j * fn(im)).astype(np.complex64)


# ---------------------------------------------------------------------------
# signal & spectrum
# ---------------------------------------------------------------------------


def _tone(freq_norm: float, n: int) -> np.ndarray:
    t = np.arange(n, dtype=np.float64)
    return np.exp(2j * np.pi * freq_norm * t).astype(np.complex64)


def _make_signal(n: int) -> np.ndarray:
    """Single full-scale complex tone at a non-bin-aligned frequency."""
    return _tone(0.07, n)


def _spectrum_db(x: np.ndarray, pad: int = 4) -> tuple[np.ndarray, np.ndarray]:
    from doppler.spectral import blackman_harris_window

    n = len(x)
    w = np.zeros(n, dtype=np.float32)
    blackman_harris_window(w)
    cg = w.mean()
    S = np.fft.fftshift(np.fft.fft(x * w, n * pad))
    amp_db = 20.0 * np.log10(np.abs(S) / (n * cg) + 1e-300)
    freq = np.fft.fftshift(np.fft.fftfreq(n * pad))
    return freq, amp_db


# ---------------------------------------------------------------------------
# plot
# ---------------------------------------------------------------------------


def _style_ax(ax) -> None:
    ax.set_facecolor("#111827")
    ax.tick_params(colors="#d1d5db")
    ax.xaxis.label.set_color("#d1d5db")
    ax.yaxis.label.set_color("#d1d5db")
    ax.title.set_color("#f1f5f9")
    for sp in ax.spines.values():
        sp.set_color("#374151")
    ax.grid(True, color="#374151", lw=0.4)


def main(out_path: str = "q15_uq15_demo.png") -> None:
    n = 65536
    x = _make_signal(n)

    xq_q15 = _cf32_apply(_q15_roundtrip, x)
    xq_uq15 = _cf32_apply(_uq15_roundtrip, x)

    # ── validation ───────────────────────────────────────────────────────────
    # The +32768 offset must cancel exactly on decode: the two conventions
    # are the same quantiser, so the decoded streams are bit-identical.
    assert np.array_equal(xq_q15, xq_uq15), (
        "Q15 and UQ15 roundtrips differ — offset did not cancel"
    )
    # Both realise the int16 quantiser SNR (6.02·16 + 1.76 ≈ 98 dB for a
    # full-scale tone); the max per-component error is one LSB, reached
    # only where the +1.0 peak clips to 32767.
    lsb = 1.0 / 32768.0
    for name, xq in (("Q15", xq_q15), ("UQ15", xq_uq15)):
        err = (xq - x).astype(np.complex128)
        snr = 10.0 * np.log10(
            float(
                np.mean(np.abs(x.astype(np.complex128)) ** 2)
                / np.mean(np.abs(err) ** 2)
            )
        )
        max_err = max(
            float(np.max(np.abs(err.real))), float(np.max(np.abs(err.imag)))
        )
        assert snr > 95.0, f"{name}: SNR {snr:.1f} dB (theory ≈ 98 dB)"
        assert max_err <= lsb * 1.001, (
            f"{name}: max error {max_err / lsb:.2f} LSB (expected ≤ 1)"
        )
        print(f"  {name}: SNR {snr:.1f} dB, max err {max_err / lsb:.2f} LSB")
    print("  Q15 == UQ15 bit-exact — OK")

    freq, amp_in = _spectrum_db(x)
    _, amp_q15 = _spectrum_db(xq_q15)
    _, amp_uq15 = _spectrum_db(xq_uq15)

    fig, (ax_in, ax_q, ax_uq) = plt.subplots(
        3, 1, figsize=(10, 11), constrained_layout=True
    )
    fig.patch.set_facecolor("#0f172a")
    fig.suptitle(
        "Q15 bipolar vs UQ15 offset-binary — Δ = 2⁻¹⁵ (identical step size)",
        fontsize=12,
        color="#f1f5f9",
    )

    # ── input ────────────────────────────────────────────────────────────────
    ax_in.plot(freq, amp_in, color="#94a3b8", lw=0.9)
    ax_in.set_xlim(-0.5, 0.5)
    ax_in.set_ylim(-130, 10)
    ax_in.set_xlabel("Normalised frequency (cycles/sample)")
    ax_in.set_ylabel("Amplitude (dBFS)")
    ax_in.set_title("Input CF32", loc="right")
    _style_ax(ax_in)

    # ── Q15 roundtrip ────────────────────────────────────────────────────────
    ax_q.plot(freq, amp_q15, color="#60a5fa", lw=0.9)
    ax_q.set_xlim(-0.5, 0.5)
    ax_q.set_ylim(-130, 10)
    ax_q.set_xlabel("Normalised frequency (cycles/sample)")
    ax_q.set_ylabel("Amplitude (dBFS)")
    ax_q.set_title("Q15 bipolar roundtrip (F32 → int16 → F32)", loc="right")
    _style_ax(ax_q)

    # ── UQ15 roundtrip ───────────────────────────────────────────────────────
    ax_uq.plot(freq, amp_uq15, color="#f97316", lw=0.9)
    ax_uq.set_xlim(-0.5, 0.5)
    ax_uq.set_ylim(-130, 10)
    ax_uq.set_xlabel("Normalised frequency (cycles/sample)")
    ax_uq.set_ylabel("Amplitude (dBFS)")
    ax_uq.set_title(
        "UQ15 offset-binary roundtrip (F32 → uint16 → F32)", loc="right"
    )
    _style_ax(ax_uq)

    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved → {out_path}")


if __name__ == "__main__":
    main()
