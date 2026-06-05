"""ddc_fn_demo.py — functional DDCR API demo (state as a PyCapsule).

The ``doppler.ddc`` package exposes two faces of the same digital
down-converter:

  * ``DDCR`` — an object wrapping the C state in a Python type, and
  * the **functional** ``ddcr_*`` API (this demo) — free functions that pass
    an opaque ``state`` capsule explicitly, so the caller owns lifetime and
    the output buffer.  Handy for pipelines that already manage their own
    arrays and want zero per-call allocation.

A DDCR takes a **real** float32 passband signal, mixes it down with a fine
NCO, low-pass filters, and **decimates** — emitting **complex64** baseband.
To park a real tone at carrier ``f_carrier`` (normalised to ``fs_in``) at DC,
set ``norm_freq = -(2 * f_carrier + 0.5)`` (the NCO runs at ``fs_in / 2``).

Shows:
  1. Lifecycle     — create → get/set → execute → reset → destroy (+ the
                     post-destroy RuntimeError guard)
  2. Tuning        — a real tone at f_carrier lands at DC after mixing
  3. Streaming     — block-by-block execute into one caller-owned buffer
  4. Spectral plot — input passband → baseband output → retuned output

Run:
  python examples/python/ddc_fn_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.ddc import (
    ddcr_create,
    ddcr_destroy,
    ddcr_execute,
    ddcr_get_norm_freq,
    ddcr_get_rate,
    ddcr_reset,
    ddcr_set_norm_freq,
)

FS_IN = 1.0  # everything normalised to the input sample rate
RATE = 0.25  # decimate 4× → fs_out = 0.25 · fs_in


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------


def _lo_for_carrier(f_carrier: float) -> float:
    """NCO setting that tunes a real tone at *f_carrier* (rel. fs_in) to DC."""
    return -(2.0 * f_carrier + 0.5)


def _real_passband(f_carrier: float, n: int, rng) -> np.ndarray:
    """Real float32 signal: a tone at *f_carrier* plus broadband noise."""
    t = np.arange(n)
    tone = np.cos(2.0 * np.pi * f_carrier * t)
    noise = 0.25 * rng.standard_normal(n)
    return (tone + noise).astype(np.float32)


def _spectrum_db(x: np.ndarray, pad: int = 8) -> tuple[np.ndarray, np.ndarray]:
    """Windowed FFT over [−0.5, +0.5]; 0 dBFS = unit-amplitude tone.

    Uses a Blackman-Harris window.  ``x`` may be real or complex; the axis is
    always the full [−0.5, +0.5) so a real input shows its mirror image.
    """
    n = len(x)
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    w = a[0] - a[1] * np.cos(k) + a[2] * np.cos(2 * k) - a[3] * np.cos(3 * k)
    cg = w.mean()
    spec = np.fft.fftshift(np.fft.fft(x * w, n * pad))
    amp_db = 20.0 * np.log10(np.abs(spec) / (n * cg) + 1e-300)
    freq = np.linspace(-0.5, 0.5, n * pad, endpoint=False)
    return freq, amp_db


def _peak_fn(x: np.ndarray) -> float:
    """Normalised frequency (cycles/sample) of the strongest spectral line."""
    freq, amp = _spectrum_db(x)
    return float(freq[int(np.argmax(amp))])


# ---------------------------------------------------------------------------
# 1. Lifecycle
# ---------------------------------------------------------------------------


def demo_lifecycle():
    print("--- 1. Lifecycle (functional API) ---")
    state = ddcr_create(_lo_for_carrier(0.18), RATE)
    print(
        f"  created      norm_freq={ddcr_get_norm_freq(state):+.4f}  "
        f"rate={ddcr_get_rate(state):.4f}"
    )

    ddcr_set_norm_freq(state, _lo_for_carrier(0.14))
    print(
        f"  retuned      norm_freq={ddcr_get_norm_freq(state):+.4f}  "
        f"(phase-continuous, no history reset)"
    )

    ddcr_reset(state)
    print("  reset        halfband / LO phase / resampler history zeroed")

    ddcr_destroy(state)
    print("  destroyed    C resources released")
    try:
        ddcr_execute(state, np.zeros(8, np.float32), np.empty(8, np.complex64))
    except RuntimeError:
        print("  use-after-destroy correctly raises RuntimeError\n")


# ---------------------------------------------------------------------------
# 2. Tuning check
# ---------------------------------------------------------------------------


def demo_tuning():
    print("--- 2. Tuning: real tone → DC ---")
    rng = np.random.default_rng(1)
    n = 16384
    print(f"  {'f_carrier':>10}  {'norm_freq':>10}  {'out peak fn':>12}")
    print(f"  {'-' * 10}  {'-' * 10}  {'-' * 12}")
    for f_carrier in (0.10, 0.18, 0.30):
        x = _real_passband(f_carrier, n, rng)
        state = ddcr_create(_lo_for_carrier(f_carrier), RATE)
        out = np.empty(n, dtype=np.complex64)
        y = np.array(ddcr_execute(state, x, out), copy=True)
        ddcr_destroy(state)
        y_settled = y[len(y) // 10 :]  # drop filter transient
        print(
            f"  {f_carrier:>10.3f}  {_lo_for_carrier(f_carrier):>+10.3f}  "
            f"{_peak_fn(y_settled):>12.4f}"
        )
    print("  (output peak ≈ 0 → the carrier is parked at DC)\n")


# ---------------------------------------------------------------------------
# 3. Block streaming into one caller-owned buffer
# ---------------------------------------------------------------------------


def demo_streaming():
    print("--- 3. Streaming: block-by-block into one buffer ---")
    rng = np.random.default_rng(2)
    n_block = 4096
    state = ddcr_create(_lo_for_carrier(0.18), RATE)
    out = np.empty(n_block, dtype=np.complex64)  # reused every block
    total_in = total_out = 0
    for _ in range(4):
        x = _real_passband(0.18, n_block, rng)
        y = ddcr_execute(state, x, out)  # zero-copy view of out[:n_out]
        total_in += len(x)
        total_out += len(y)
    ddcr_destroy(state)
    print(
        f"  fed {total_in} real samples in 4 blocks → "
        f"{total_out} complex samples  (≈{total_in / total_out:.1f}× "
        f"decimation)\n"
    )


# ---------------------------------------------------------------------------
# 4. Spectral plot
# ---------------------------------------------------------------------------


def demo_spectral_plot(out_path="ddc_fn_demo.png"):
    """Save a 3-panel spectral plot: input passband, baseband, retuned.

    Input: real noise + tone at f_carrier=0.18 (rel. fs_in).  Panel 2 mixes
    with the LO that parks 0.18 at DC; panel 3 retunes the *same* stream's LO
    to 0.14, shifting the tone off DC by (0.18 − 0.14)/rate in fs_out units.
    """
    rng = np.random.default_rng(42)
    n = 16384
    f_carrier = 0.18
    x = _real_passband(f_carrier, n, rng)

    # baseband output (LO parks the carrier at DC)
    s_dc = ddcr_create(_lo_for_carrier(f_carrier), RATE)
    y_dc = np.array(
        ddcr_execute(s_dc, x, np.empty(n, np.complex64)), copy=True
    )
    ddcr_destroy(s_dc)

    # retuned output (LO 0.04 below the carrier → tone lands above DC)
    s_off = ddcr_create(_lo_for_carrier(0.14), RATE)
    y_off = np.array(
        ddcr_execute(s_off, x, np.empty(n, np.complex64)), copy=True
    )
    ddcr_destroy(s_off)

    drop = lambda y: y[len(y) // 10 :]  # noqa: E731 — drop transient
    panels = [
        (
            x,
            "#60a5fa",
            f"Input — real passband, tone at fn=±{f_carrier}",
            "fs_in",
            None,
        ),
        (
            drop(y_dc),
            "#4ade80",
            "DDCR output — LO tuned to carrier (tone at DC)",
            "fs_out",
            0.0,
        ),
        (
            drop(y_off),
            "#fbbf24",
            "DDCR output — LO retuned 0.04 below carrier",
            "fs_out",
            None,
        ),
    ]

    fig, axes = plt.subplots(3, 1, figsize=(10, 7.2), constrained_layout=True)
    fig.suptitle(
        "doppler — functional DDCR API (ddcr_create / ddcr_execute)\n"
        f"real {n}-sample input → complex baseband, {int(1 / RATE)}× "
        "decimation",
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

    for ax, (sig, color, title, unit, mark) in zip(axes, panels):
        freq, amp = _spectrum_db(sig)
        ax.plot(freq, amp, color=color, lw=0.8)
        peak = _peak_fn(sig)
        ax.axvline(peak, color="#f87171", lw=1.1, linestyle="--", alpha=0.9)
        ax.annotate(
            f"peak fn={peak:+.3f}",
            xy=(peak, 0),
            xytext=(peak + 0.06, -18),
            color="#f87171",
            fontsize=9,
            arrowprops=dict(arrowstyle="->", color="#f87171", lw=1.0),
        )
        ax.set_xlim(-0.5, 0.5)
        ax.set_ylim(-90, 10)
        ax.set_ylabel("dBFS")
        ax.set_title(
            f"{title}  ({len(sig)} samples)", loc="right", color="#f1f5f9"
        )
        ax.text(
            -0.48,
            5,
            f"x-axis: cycles/sample of {unit}",
            color="#9ca3af",
            fontsize=8,
            va="top",
            bbox=dict(
                boxstyle="round,pad=0.2",
                facecolor="#1f2937",
                edgecolor="#4b5563",
            ),
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
    print("=== doppler functional DDCR (ddc_fn) demo ===\n")
    demo_lifecycle()
    demo_tuning()
    demo_streaming()
    demo_spectral_plot()
    print("Done.")
