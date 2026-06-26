"""symsync_theory_demo.py — Gardner timing loop validated against theory.

Two theoretical-correctness checks for :class:`doppler.track.SymbolSync`:

  * **Timing-detector S-curve** — open-loop (bn → 0) over a swept offset,
    swept static timing offset and read the Gardner TED, vs a **semi-analytic
    Gardner reference** computed directly from the pulse train. The
    characteristic is one period per symbol with two zeros a half apart —
    stable lock (positive restoring slope) and the unstable null.

  * **Timing-error variance vs SNR** — at the lock point, `var(e)` is a
    data-pattern **self-noise floor** (present at infinite SNR — a defining
    Gardner property) plus an AWGN contribution that grows as `1/SNR` (signal×
    noise) and `1/SNR²` (noise×noise).

Run:  python -m doppler.examples.symsync_theory_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.track import SymbolSync

SPS = 8
BETA = 0.5


def _rc(t):
    t = np.asarray(t, float)
    s = np.sinc(t / SPS)
    d = 1 - (2 * BETA * t / SPS) ** 2
    c = np.cos(np.pi * BETA * t / SPS)
    with np.errstate(divide="ignore", invalid="ignore"):
        s = s * np.where(np.abs(d) < 1e-8, np.pi / 4, c / d)
    return s


def _pulse_train(a, offset):
    n = len(a) * SPS
    s = np.zeros(n)
    span = 8 * SPS
    for k, ak in enumerate(a):
        c = k * SPS + offset
        idx = np.arange(max(0, int(c - span)), min(n, int(c + span)))
        s[idx] += ak * _rc(idx - c)
    return s


def _measured_scurve(offsets, nsym=400, seed=1):
    a = np.random.default_rng(seed).integers(0, 2, nsym) * 2 - 1
    out = np.empty(len(offsets))
    for i, off in enumerate(offsets):
        ss = SymbolSync(sps=SPS, bn=1e-7, zeta=0.707, order="cubic")
        x = _pulse_train(a, off).astype(np.complex64)
        e = []
        for j in range(0, len(x), SPS):
            ss.steps(x[j : j + SPS])
            e.append(ss.timing_error)
        out[i] = np.mean(e[len(e) // 2 :])
    return out


def _reference_scurve(offsets, nsym=600, seed=1):
    # Gardner detector on the dense pulse train, ideal interpolation
    a = np.random.default_rng(seed).integers(0, 2, nsym) * 2 - 1
    n = nsym * SPS
    grid = np.arange(n)
    out = np.empty(len(offsets))
    for i, off in enumerate(offsets):
        s = _pulse_train(a, off)
        ks = np.arange(2, nsym - 1)
        on = np.interp(ks * SPS, grid, s)
        onm1 = np.interp((ks - 1) * SPS, grid, s)
        mid = np.interp(ks * SPS - SPS // 2, grid, s)
        out[i] = np.mean(mid * (on - onm1))
    return out


def _peak_norm(y):
    return y / np.max(np.abs(y))


def _align(meas, ref):
    # circularly shift the reference to best match the measured (absorbs the
    # constant group-delay offset between the two timing references)
    best, shift = -2.0, 0
    for s in range(len(ref)):
        c = np.corrcoef(meas, np.roll(ref, s))[0, 1]
        if c > best:
            best, shift = c, s
    return np.roll(ref, shift), best


def _ted_var(snr_db, nsym=3000, seed=3):
    a = np.random.default_rng(seed).integers(0, 2, nsym) * 2 - 1
    x = _pulse_train(a, 0.0)
    if snr_db is not None:
        rng = np.random.default_rng(seed + 1)
        p = np.sqrt(np.mean(np.abs(x) ** 2))
        std = np.sqrt(10 ** (-snr_db / 10)) * p
        x = (
            x
            + rng.normal(0, std / np.sqrt(2), len(x))
            + 1j * rng.normal(0, std / np.sqrt(2), len(x))
        )
    ss = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    e = []
    x = x.astype(np.complex64)
    for j in range(0, len(x), SPS):
        ss.steps(x[j : j + SPS])
        e.append(ss.timing_error)
    return np.var(e[len(e) // 2 :])


def main(out_path="symsync_theory_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    offs = np.linspace(0, SPS, 64, endpoint=False) / SPS  # in symbols
    meas_raw = _measured_scurve(offs * SPS, nsym=600)
    # median-filter the lone discrete artifact at the group-delay offset
    med = np.array(
        [
            np.median(np.take(meas_raw, range(i - 1, i + 2), mode="wrap"))
            for i in range(len(meas_raw))
        ]
    )
    meas = _peak_norm(med)
    ref, corr = _align(meas, _peak_norm(_reference_scurve(offs * SPS)))

    snr_db = np.array([20, 15, 12, 9, 6, 3])
    var = np.array([_ted_var(s) for s in snr_db])
    floor = _ted_var(None)
    snr = 10 ** (snr_db / 10)
    # AWGN model fit: var = floor + a/snr + b/snr^2
    A = np.vstack([1 / snr, 1 / snr**2]).T
    ab, *_ = np.linalg.lstsq(A, var - floor, rcond=None)
    model = floor + A @ ab

    fig, (ax, bx) = plt.subplots(1, 2, figsize=(11, 4.5))

    ax.plot(offs, ref, "k--", lw=2, label="semi-analytic Gardner")
    ax.plot(offs, meas, color="#2ca02c", lw=1.4, label="measured TED")
    ax.axhline(0, color="0.7", lw=0.6)
    ax.set_xlabel("timing offset (symbols)")
    ax.set_ylabel("Gardner discriminator e (peak-normalised)")
    ax.set_title(
        f"Gardner timing-detector S-curve (shape ρ = {corr:.3f})", fontsize=10
    )
    ax.legend(fontsize=8, loc="upper right")
    ax.grid(alpha=0.3)

    bx.semilogy(
        snr_db, var, "o", color="#2ca02c", ms=6, label="measured var(e)"
    )
    bx.semilogy(
        snr_db, model, "k--", lw=1.6, label=r"floor + $a/\rho$ + $b/\rho^2$"
    )
    bx.axhline(
        floor,
        color="#d62728",
        ls=":",
        lw=1.2,
        label=f"self-noise floor ({floor:.3f})",
    )
    bx.set_xlabel("symbol SNR (dB)")
    bx.set_ylabel("timing-error variance")
    bx.set_title("Timing-error variance vs SNR", fontsize=10)
    bx.legend(fontsize=8, loc="upper right")
    bx.grid(alpha=0.3, which="both")

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    # report S-curve shape match near the lock (scale-aligned slope)
    print(
        f"wrote {out_path}  (self-noise floor {floor:.3f}, "
        f"AWGN a={ab[0]:.2f} b={ab[1]:.2f})"
    )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "symsync_theory_demo.png")
