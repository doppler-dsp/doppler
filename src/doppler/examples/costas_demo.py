"""costas_demo.py — carrier-loop stress vs time for a given carrier residual.

Drives :class:`doppler.track.Costas` with a continuous BPSK signal carrying a
**residual** carrier offset that *moves* — a constant offset plus a Doppler
**ramp** (the dynamic stress a real receiver sees after FFT acquisition has
removed the bulk Doppler). The same scenario is run at three loop bandwidths so
you can see the classic tracking trade-off.

Three views (saved to a PNG):
  * **Frequency tracking** — the NCO frequency estimate (per loop bandwidth)
    riding the true moving residual (black dashed).
  * **Loop stress vs time** — the per-symbol Costas phase-discriminator error
    (degrees): a large *transient* stress during pull-in that decays to a small
    *dynamic* stress set by the ramp. A wider loop sheds dynamic stress (lower
    floor) at the cost of more noise; a narrow loop is quiet but stressed.
  * **Lock metric vs time** — |Re P|/|P| ramping to 1 as each loop locks.

Run:  python -m doppler.examples.costas_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.track import Costas

TSAMPS = 16  # samples per symbol (integrate-and-dump period)
NSYM = 1800  # symbols
F0 = 0.002  # residual offset at t=0, cycles/sample (the step to acquire)
RAMP = 1.5e-8  # Doppler rate, cycles/sample per sample (the dynamic stress)
SNR_DB = 15.0  # per-sample SNR
BWS = [0.03, 0.06, 0.12]  # loop noise bandwidths to compare


def _signal(seed=0):
    """Continuous BPSK with a moving carrier residual (F0 + RAMP) + AWGN.

    Returns (rx, true_freq_per_symbol).
    """
    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, NSYM) * 2 - 1
    sig = np.repeat(bits.astype(np.complex64), TSAMPS)
    k = np.arange(len(sig), dtype=np.float64)
    inst_freq = F0 + RAMP * k  # instantaneous frequency per sample
    phase = 2.0 * np.pi * np.cumsum(inst_freq)
    rx = sig * np.exp(1j * phase)
    sigma = np.sqrt(10.0 ** (-SNR_DB / 10.0) / 2.0)
    rx = rx + (
        rng.normal(0, sigma, len(rx)) + 1j * rng.normal(0, sigma, len(rx))
    )
    # true residual at each symbol's center
    centers = (np.arange(NSYM) + 0.5) * TSAMPS
    return rx.astype(np.complex64), F0 + RAMP * centers


def _track(rx, bn):
    """Run a Costas loop symbol-by-symbol, recording per-symbol diagnostics."""
    c = Costas(bn=bn, zeta=0.707, init_norm_freq=0.0, tsamps=TSAMPS)
    freq = np.empty(NSYM)
    stress = np.empty(NSYM)
    lock = np.empty(NSYM)
    for s in range(NSYM):
        c.steps(rx[s * TSAMPS : (s + 1) * TSAMPS])
        freq[s] = c.norm_freq
        stress[s] = c.last_error  # radians (discriminator output)
        lock[s] = c.lock_metric
    return freq, stress, lock


def main(out_path="costas_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rx, true_freq = _signal()
    t = np.arange(NSYM)
    colors = ["#1f77b4", "#2ca02c", "#d62728"]
    runs = {bn: _track(rx, bn) for bn in BWS}

    fig, (a, b, c) = plt.subplots(3, 1, figsize=(9, 8), sharex=True)

    a.plot(t, true_freq * 1e3, "k--", lw=1.4, label="true residual")
    for bn, col in zip(BWS, colors):
        a.plot(t, runs[bn][0] * 1e3, color=col, lw=1.0, label=f"Bn={bn}")
    a.set_ylabel("freq (millicycles/sample)")
    a.set_title(
        f"Carrier loop: acquire a {F0 * 1e3:.0f}-millicycle step, then "
        f"track a Doppler ramp (SNR {SNR_DB:.0f} dB)",
        fontsize=10,
    )
    a.legend(fontsize=8, ncol=4, loc="upper left")
    a.grid(alpha=0.3)

    def rms(x, w=40):
        """Sliding-window RMS: the stress envelope, not per-symbol noise."""
        k = np.ones(w) / w
        return np.sqrt(np.convolve(x * x, k, mode="same"))

    for bn, col in zip(BWS, colors):
        b.plot(
            t,
            np.degrees(rms(runs[bn][1])),
            color=col,
            lw=1.2,
            label=f"Bn={bn}",
        )
    b.set_ylabel("RMS phase error (deg)")
    b.set_yscale("log")
    b.set_title(
        "Loop stress vs time — wide Bn pulls the step in fast (short "
        "transient); narrow Bn rings, then all settle to a low floor",
        fontsize=10,
    )
    b.legend(fontsize=8, ncol=3, loc="upper right")
    b.grid(alpha=0.3, which="both")

    for bn, col in zip(BWS, colors):
        c.plot(t, runs[bn][2], color=col, lw=1.0, label=f"Bn={bn}")
    c.set_ylabel("lock metric")
    c.set_xlabel("symbol")
    c.set_title("Lock metric vs time", fontsize=10)
    c.set_ylim(0, 1.05)
    c.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "costas_demo.png")
