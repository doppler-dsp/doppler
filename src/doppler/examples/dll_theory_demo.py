"""dll_theory_demo.py — DLL code loop validated against theory.

Two theoretical-correctness checks for :class:`doppler.track.Dll`'s
non-coherent early-minus-late code discriminator `(|E|-|L|)/(|E|+|L|)`:

  * **Code-detector S-curve** — drive the loop open-loop (bn -> 0) over a swept
    static code-phase error and read the discriminator, vs the triangular
    autocorrelation reference, R(t)=max(0,1-|t|): zero at the lock t=0
    with a restoring (negative) slope.
    Zero (stable lock) at t=0 with a restoring (negative) slope. The
    fractional-boundary integrate-and-dump (overlap-weighting the lone sample
    that straddles a chip transition) makes the curve smooth and antisymmetric
    to round-off at any sps — no integer-sample code-phase staircase.

  * **Code-error variance vs SNR** — at lock the early-late discriminator
    variance follows a clean `1/SNR` law (the per-epoch code-error noise).

Run:  python -m doppler.examples.dll_theory_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.track import Dll
from doppler.wfm import PN, mls_poly

N = 7
L = (1 << N) - 1


def _code():
    c = np.asarray(PN(poly=mls_poly(N), seed=1, length=N).generate(L))
    return c.astype(np.uint8), np.where(c.astype(int) & 1, -1.0, 1.0)


def _scurve(sps, taus, spacing=0.5):
    code, csign = _code()
    sig = np.tile(np.repeat(csign, sps).astype(np.complex64), 6)
    out = np.empty(len(taus))
    for i, t in enumerate(taus):
        d = Dll(code, sps, float(t), 1e-7, 0.707, spacing)
        d.steps(sig)
        out[i] = d.last_error
    return out


def _tri_ref(taus, s=0.5):
    def acorr(t):
        return np.maximum(0, 1 - np.abs(t))

    early, late = acorr(taus + s), acorr(taus - s)
    return (early - late) / (early + late + 1e-12)


def _disc_var(snr_db, sps=16, nper=400):
    code, csign = _code()
    base = np.repeat(csign, sps).astype(np.complex64)
    rng = np.random.default_rng(7)
    x = np.tile(base, nper).copy()
    p = np.sqrt(np.mean(np.abs(x) ** 2))
    std = np.sqrt(10 ** (-snr_db / 10)) * p
    x = x + (
        rng.normal(0, std / np.sqrt(2), x.size)
        + 1j * rng.normal(0, std / np.sqrt(2), x.size)
    ).astype(np.complex64)
    d = Dll(code, sps, 0.0, 1e-7, 0.707, 0.5)
    e = []
    for i in range(0, len(x), L * sps):
        d.steps(x[i : i + L * sps])
        e.append(d.last_error)
    return np.var(e[len(e) // 2 :])


def main(out_path="dll_theory_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    taus = np.linspace(-1, 1, 81)
    meas = _scurve(16, taus)
    ref = _tri_ref(taus)

    snr_db = np.array([20, 15, 10, 5, 0])
    var = np.array([_disc_var(s) for s in snr_db])
    snr = 10 ** (snr_db / 10)
    law = var[0] * snr[0] / snr  # 1/SNR through the 20 dB anchor

    fig, (a, b) = plt.subplots(1, 2, figsize=(11, 4.5))

    a.plot(taus, ref, "k--", lw=2, label="triangular E-L reference")
    a.plot(taus, meas, color="#9467bd", lw=1.4, label="measured (sps=16)")
    a.axhline(0, color="0.7", lw=0.6)
    a.axvline(0, color="0.7", lw=0.6)
    a.set_xlabel("code-phase error (chips)")
    a.set_ylabel("discriminator (|E|-|L|)/(|E|+|L|)")
    a.set_title("DLL code-detector S-curve (stable lock at 0)", fontsize=10)
    a.legend(fontsize=8, loc="upper right")
    a.grid(alpha=0.3)

    b.loglog(snr, law, "k--", lw=2, label="1/SNR law")
    b.loglog(snr, var, "o", color="#9467bd", ms=6, label="measured var")
    b.set_xlabel("per-epoch SNR (linear)")
    b.set_ylabel("code-error variance")
    b.set_title("Code-error variance vs SNR (1/SNR)", fontsize=10)
    b.legend(fontsize=8, loc="upper right")
    b.grid(alpha=0.3, which="both")

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    asym = np.max(np.abs(meas + meas[::-1]))
    print(
        f"wrote {out_path}  (e(0)={meas[len(meas) // 2]:.2e}, "
        f"antisym_err={asym:.3f}, var.SNR≈{np.mean(var * snr):.2e})"
    )

    # ── self-validation: the demo's theory claims, asserted ──────────────
    # Stable lock: the discriminator must null exactly at zero code-phase
    # error and be antisymmetric to round-off (the fractional-boundary
    # integrate-and-dump leaves no integer-sample staircase bias).
    assert abs(meas[len(meas) // 2]) < 1e-6, "discriminator not null at 0"
    assert asym < 1e-4, "S-curve is not antisymmetric"
    # The measured curve must follow the triangular-autocorrelation E-L
    # reference across the swept chip range (restoring slope included).
    scurve_err = np.max(np.abs(meas - ref))
    print(f"S-curve max err vs triangular reference {scurve_err:.3f}")
    assert scurve_err < 0.05, "S-curve departs from the triangular model"
    # At lock the code-error variance is thermal: every swept point must
    # sit on the 1/SNR line (anchored at 20 dB) to within 1 dB.
    dev_db = np.abs(10 * np.log10(var / law))
    print(f"var vs 1/SNR law: max dev {dev_db.max():.2f} dB")
    assert np.all(dev_db < 1.0), "code-error variance departs from 1/SNR"


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "dll_theory_demo.png")
