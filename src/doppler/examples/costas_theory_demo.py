"""costas_theory_demo.py — Costas loop validated against theory.

Two theoretical-correctness checks for :class:`doppler.track.Costas`'s
decision-directed BPSK phase discriminator `e = sign(Re P)·Im(P)/|P|`:

  * **Phase-detector S-curve** — drive the loop open-loop (bn → 0) with a
    noiseless prompt `exp(jφ)` at a swept static phase error φ and read the
    discriminator. It must trace the analytical characteristic
    `e(φ) = sign(cos φ)·sin φ`: zero (stable lock) at φ = 0, the 180° data
    ambiguity at ±180°, the unstable nulls at ±90°, and unit slope (Kd = 1) at
    lock.

  * **Discriminator (phase-error) variance vs SNR** — at φ = 0, feed noisy
    prompts `±1 + n` and measure `var(e)`. At high SNR it follows the BPSK
    squaring-loss law `σ_e² = 1/(2·SNR)·(1 + 1/(2·SNR))`; because doppler's
    discriminator is normalised by |P| it is bounded and falls below the
    (divergent) law at low SNR — shown for honesty.

Run:  python -m doppler.examples.costas_theory_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.track import Costas


def _scurve(phis):
    meas = np.empty(len(phis))
    for i, phi in enumerate(phis):
        c = Costas(
            bn=1e-6, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0
        )
        c.steps(np.array([np.exp(1j * phi)], np.complex64))
        meas[i] = c.last_error
    return meas


def _disc_var(snr_db, n=120000, seed=1):
    rng = np.random.default_rng(seed)
    snr = 10 ** (snr_db / 10)
    sigma = np.sqrt(1.0 / snr)
    d = rng.integers(0, 2, n) * 2 - 1
    P = (
        d
        + rng.normal(0, sigma / np.sqrt(2), n)
        + 1j * rng.normal(0, sigma / np.sqrt(2), n)
    ).astype(np.complex64)
    c = Costas(bn=1e-7, zeta=0.707, init_norm_freq=0.0, tsamps=1, bn_fll=0.0)
    e = np.empty(n)
    for k in range(n):
        c.steps(P[k : k + 1])
        e[k] = c.last_error
    return np.var(e)


def main(out_path="costas_theory_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    phis = np.linspace(-np.pi, np.pi, 181)
    meas = _scurve(phis)
    theory = np.where(np.cos(phis) >= 0, np.sin(phis), -np.sin(phis))

    snr_db = np.array([20, 16, 12, 9, 6, 3, 0])
    mv = np.array([_disc_var(s) for s in snr_db])
    snr = 10 ** (snr_db / 10)
    law = (1 / (2 * snr)) * (1 + 1 / (2 * snr))

    fig, (a, b) = plt.subplots(1, 2, figsize=(11, 4.5))

    a.plot(
        np.degrees(phis), theory, "k--", lw=2, label="theory  sign(cosφ)·sinφ"
    )
    a.plot(
        np.degrees(phis),
        meas,
        color="#1f77b4",
        lw=1.3,
        label="measured discriminator",
    )
    a.axhline(0, color="0.7", lw=0.6)
    for x in (-90, 90):
        a.axvline(x, color="#d62728", ls=":", lw=0.8)
    a.set_xlabel("phase error φ (deg)")
    a.set_ylabel("discriminator e")
    a.set_title("Costas phase-detector S-curve (Kd = 1 at lock)", fontsize=10)
    a.set_xticks([-180, -90, 0, 90, 180])
    a.legend(fontsize=8, loc="upper right")
    a.grid(alpha=0.3)

    b.semilogy(
        snr_db,
        law,
        "k--",
        lw=2,
        label=r"squaring loss $\frac{1}{2\rho}(1+\frac{1}{2\rho})$",
    )
    b.semilogy(
        snr_db, mv, "o-", color="#1f77b4", ms=5, label="measured var(e)"
    )
    b.set_xlabel("prompt SNR (dB)")
    b.set_ylabel("discriminator variance")
    b.set_title(
        "Phase-error variance vs SNR (normalised → bounded)", fontsize=10
    )
    b.legend(fontsize=8, loc="upper right")
    b.grid(alpha=0.3, which="both")

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    err = np.max(np.abs(meas - theory))
    hi = snr_db >= 10
    print(
        f"wrote {out_path}  (S-curve max err {err:.2e}, "
        f"hi-SNR var ratio {np.mean(mv[hi] / law[hi]):.3f})"
    )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "costas_theory_demo.png")
