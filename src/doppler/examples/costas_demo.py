"""costas_demo.py — carrier-loop stress vs time, and why FLL assist matters.

Drives :class:`doppler.track.Costas` with a continuous BPSK signal carrying a
**large residual** carrier offset (~0.9 rad/symbol) — bigger than a bare Costas
PLL can pull in, whatever its loop bandwidth. Three configurations run the same
scenario:

  * a narrow PLL (Bn = 0.01, no FLL),
  * a wide PLL (Bn = 0.10, no FLL),
  * an **FLL-assisted** PLL (Bn = 0.01, bn_fll = 0.03).

Three views (saved to a PNG):
  * **Frequency tracking** — the NCO estimate vs the true residual (black
    dashed). Both bare PLLs stall near zero (the phase discriminator's pull-in
    is far narrower than the residual); the FLL-assisted loop snaps on.
  * **Loop stress vs time** — sliding-RMS of the phase-discriminator error. The
    bare PLLs stay pinned at max stress (never locked); the FLL assist drives
    the stress down as its wide cross-product frequency discriminator pulls the
    integrator onto the residual.
  * **Lock metric vs time** — |Re P|/|P|: stuck near 0.6 (no lock) for the bare
    PLLs, ramping to 1 for the FLL-assisted loop.

Run:  python -m doppler.examples.costas_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:loop]
import numpy as np

from doppler.track import Costas

TSAMPS = 16  # samples per symbol (integrate-and-dump period)
NSYM = 4000  # symbols
F0 = 0.009  # large residual carrier offset, cycles/sample (~0.9 rad/symbol)
SNR_DB = 15.0  # per-sample SNR

# Continuous BPSK carrying the residual carrier offset F0 + AWGN.
rng = np.random.default_rng(0)
bits = rng.integers(0, 2, NSYM) * 2 - 1
sig = np.repeat(bits.astype(np.complex64), TSAMPS)
k = np.arange(len(sig))
rx = sig * np.exp(2j * np.pi * F0 * k)
sigma = np.sqrt(10.0 ** (-SNR_DB / 10.0) / 2.0)
rx = rx + (rng.normal(0, sigma, len(rx)) + 1j * rng.normal(0, sigma, len(rx)))
rx = rx.astype(np.complex64)

# FLL-assisted PLL: the wide cross-product frequency discriminator pulls
# the loop's integrator onto the large residual; the PLL refines phase.
c = Costas(bn=0.01, zeta=0.707, init_norm_freq=0.0, tsamps=TSAMPS, bn_fll=0.03)
symbols = c.steps(rx)  # one complex prompt per symbol
f_est = c.norm_freq  # tracked residual frequency (cycles/sample)
locked = c.lock_metric  # |Re P|/|P| EMA, ~1.0 when phase-locked
# --8<-- [end:loop]

# The narrative run above must itself acquire and lock — the same
# physics the three-config sweep asserts in detail inside main().
assert abs(f_est - F0) < 5e-4, "narrative FLL+PLL run did not converge"
assert locked > 0.95, "narrative FLL+PLL run did not phase-lock"

# (label, bn, bn_fll)
CONFIGS = [
    ("PLL  Bn=0.01", 0.01, 0.0),
    ("PLL  Bn=0.10", 0.10, 0.0),
    ("FLL+PLL Bn=0.01", 0.01, 0.03),
]


def _track(rx, bn, bn_fll):
    """Run a Costas loop symbol-by-symbol, recording per-symbol diagnostics."""
    c = Costas(
        bn=bn, zeta=0.707, init_norm_freq=0.0, tsamps=TSAMPS, bn_fll=bn_fll
    )
    freq = np.empty(NSYM)
    stress = np.empty(NSYM)
    lock = np.empty(NSYM)
    for s in range(NSYM):
        c.steps(rx[s * TSAMPS : (s + 1) * TSAMPS])
        freq[s] = c.norm_freq
        stress[s] = c.last_error
        lock[s] = c.lock_metric
    return freq, stress, lock


def main(out_path="costas_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    t = np.arange(NSYM)
    colors = ["#1f77b4", "#ff7f0e", "#2ca02c"]
    runs = {lbl: _track(rx, bn, bf) for lbl, bn, bf in CONFIGS}

    # ── self-validation: the demo's physics, asserted ────────────────────
    # Tail = the last 1000 symbols, well past the FLL pull-in transient.
    tail = slice(NSYM - 1000, None)
    f_fll, s_fll, l_fll = runs["FLL+PLL Bn=0.01"]
    freq_err = abs(float(np.mean(f_fll[tail])) - F0)
    lock_fll = float(np.mean(l_fll[tail]))
    rms_fll = np.degrees(np.sqrt(np.mean(s_fll[tail] ** 2)))
    print(
        f"FLL+PLL tail: freq err {freq_err:.2e} cyc/sample, "
        f"lock {lock_fll:.3f}, RMS phase err {rms_fll:.2f} deg"
    )
    # The FLL-assisted loop must pull onto the true residual and lock.
    assert freq_err < 5e-4, "FLL-assisted loop did not converge on F0"
    assert lock_fll > 0.95, "FLL-assisted loop did not phase-lock"
    assert rms_fll < 5.0, "FLL-assisted loop stress did not settle"
    # Both bare PLLs must stall (the demo's point): the residual sits far
    # outside the phase discriminator's pull-in range at either bandwidth,
    # so the estimate stays near zero and the lock metric never rises.
    for lbl in ("PLL  Bn=0.01", "PLL  Bn=0.10"):
        f_b, s_b, l_b = runs[lbl]
        assert abs(float(np.mean(f_b[tail]))) < 0.5 * F0, (
            f"{lbl} unexpectedly pulled in"
        )
        assert float(np.mean(l_b[tail])) < 0.8, f"{lbl} unexpectedly locked"
        assert np.degrees(np.sqrt(np.mean(s_b[tail] ** 2))) > 20.0, (
            f"{lbl} stress unexpectedly low for an unlocked loop"
        )

    fig, (a, b, c) = plt.subplots(3, 1, figsize=(9, 8), sharex=True)

    a.axhline(F0 * 1e3, color="k", ls="--", lw=1.4, label="true residual")
    for (lbl, _, _), col in zip(CONFIGS, colors):
        a.plot(t, runs[lbl][0] * 1e3, color=col, lw=1.1, label=lbl)
    a.set_ylabel("freq (millicycles/sample)")
    a.set_title(
        f"Large residual ({F0 * TSAMPS * 2 * np.pi:.1f} rad/symbol): bare PLL "
        f"stalls, FLL assist pulls in (SNR {SNR_DB:.0f} dB)",
        fontsize=10,
    )
    a.legend(fontsize=8, ncol=4, loc="center right")
    a.grid(alpha=0.3)

    def rms(x, w=40):
        """Sliding-window RMS: the stress envelope, not per-symbol noise."""
        k = np.ones(w) / w
        return np.sqrt(np.convolve(x * x, k, mode="same"))

    for (lbl, _, _), col in zip(CONFIGS, colors):
        b.plot(t, np.degrees(rms(runs[lbl][1])), color=col, lw=1.2, label=lbl)
    b.set_ylabel("RMS phase error (deg)")
    b.set_title(
        "Loop stress vs time — bare PLLs stay pinned at max stress (no lock); "
        "the FLL assist drives it down",
        fontsize=10,
    )
    b.legend(fontsize=8, ncol=3, loc="center right")
    b.grid(alpha=0.3)

    for (lbl, _, _), col in zip(CONFIGS, colors):
        c.plot(t, runs[lbl][2], color=col, lw=1.1, label=lbl)
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
