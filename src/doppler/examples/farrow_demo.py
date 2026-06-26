"""farrow_demo.py — Farrow interpolator frequency response by order.

Drives :class:`doppler.resample.Farrow` at a half-sample fractional delay
(mu = 0.5, the worst case) with complex tones swept across the band, and
measures the interpolator's magnitude and delay-error response for each order
(linear / parabolic / cubic). Higher order = flatter response further up the
band — the classic interpolator trade-off.

Two views (saved to a PNG):
  * **Magnitude response** — |H(f)| in dB. An ideal fractional delay is 0 dB
    everywhere; each interpolator droops at high frequency, cubic the least.
  * **Group-delay error** — the deviation of the realised delay from the
    requested mu, in samples, vs frequency.

Run:  python -m doppler.examples.farrow_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.resample import Farrow

ORDERS = ["linear", "parabolic", "cubic"]
MU = 0.5
N = 4096


def _response(order, freqs):
    """Return (|H|, delay_error_samples) at each normalised frequency."""
    mag = np.empty(len(freqs))
    derr = np.empty(len(freqs))
    f = Farrow(order=order)
    gd = f.group_delay
    for i, fn in enumerate(freqs):
        f.reset()
        x = np.exp(2j * np.pi * fn * np.arange(N)).astype(np.complex64)
        y = f.delay(x, MU)
        sl = slice(N // 4, N - 4)  # steady-state
        n = np.arange(N)[sl]
        ideal = np.exp(2j * np.pi * fn * (n - gd + MU))
        ratio = y[sl] / ideal  # |.| = magnitude, angle = phase error
        mag[i] = np.mean(np.abs(ratio))
        # phase error -> delay error (samples): dphi = -2 pi f * d_delay
        ph = np.angle(np.mean(ratio))
        derr[i] = -ph / (2 * np.pi * fn) if fn > 0 else 0.0
    return mag, derr


def main(out_path="farrow_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    freqs = np.linspace(0.001, 0.48, 120)
    colors = {"linear": "#1f77b4", "parabolic": "#ff7f0e", "cubic": "#2ca02c"}

    fig, (a, b) = plt.subplots(2, 1, figsize=(9, 7), sharex=True)
    for o in ORDERS:
        mag, derr = _response(o, freqs)
        a.plot(
            freqs,
            20 * np.log10(np.maximum(mag, 1e-6)),
            color=colors[o],
            lw=1.4,
            label=o,
        )
        b.plot(freqs, derr, color=colors[o], lw=1.4, label=o)

    a.axhline(0, color="0.6", lw=0.7, ls="--")
    a.set_ylabel("|H(f)| (dB)")
    a.set_ylim(-6, 1)
    a.set_title(
        f"Farrow magnitude response at mu={MU} — linear droops first; "
        f"all three are delay-bias-free (below)",
        fontsize=10,
    )
    a.legend(fontsize=9, loc="lower left")
    a.grid(alpha=0.3)

    b.set_ylabel("delay error (samples)")
    b.set_xlabel("normalised frequency (cycles/sample)")
    b.set_title("Group-delay error vs frequency", fontsize=10)
    b.legend(fontsize=9, loc="lower left")
    b.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "farrow_demo.png")
