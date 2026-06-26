"""symsync_demo.py — Gardner symbol-timing recovery on asynchronous data.

Builds an RC-shaped BPSK stream whose symbol clock is offset both in phase and
in *rate* (a 0.4% fast clock — asynchronous to the receiver's sample clock) and
buried in noise, then runs :class:`doppler.track.SymbolSync`. The synchronizer
drives an **integer timing NCO** whose post-wrap value is the interpolation
fraction (no floating-point timing phase), feeds a Farrow interpolator, and
closes a Gardner-TED PI loop on the resulting symbol-rate samples.

Three views (saved to a PNG):
  * **Recovered symbols** — the symbol's real part per index: a brief pull-in,
    then clean +/-1 once timing locks (amb-BER annotated).
  * **Tracked clock** — the recovered samples/symbol converging onto the true
    offset rate (dashed) — the loop tracks the *asynchronous* clock, not just a
    static phase.
  * **Eye diagram** — the oversampled input folded to two symbols.

Run:  python -m doppler.examples.symsync_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.track import SymbolSync

SPS = 4
BETA = 0.35
NSYM = 2500
CLOCK_RATE = 1.004  # 0.4% fast symbol clock (asynchronous)
OFFSET = 1.7  # static fractional-sample timing offset
SNR_DB = 14.0


def _rc(t, beta, T):
    t = np.asarray(t, float)
    s = np.sinc(t / T)
    denom = 1 - (2 * beta * t / T) ** 2
    cos = np.cos(np.pi * beta * t / T)
    with np.errstate(divide="ignore", invalid="ignore"):
        s = s * np.where(np.abs(denom) < 1e-8, np.pi / 4, cos / denom)
    return s


def _signal(seed=0):
    rng = np.random.default_rng(seed)
    a = rng.integers(0, 2, NSYM) * 2 - 1
    n = NSYM * SPS
    s = np.zeros(n)
    span = 8 * SPS
    for k, ak in enumerate(a):
        c = k * SPS * CLOCK_RATE + OFFSET
        if c + span >= n:
            break
        idx = np.arange(max(0, int(c - span)), min(n, int(c + span)))
        s[idx] += ak * _rc(idx - c, BETA, SPS)
    s = s.astype(np.complex64)
    p = np.sqrt(np.mean(np.abs(s) ** 2))
    std = np.sqrt(10 ** (-SNR_DB / 10)) * p
    s = s + (
        rng.normal(0, std / np.sqrt(2), n)
        + 1j * rng.normal(0, std / np.sqrt(2), n)
    ).astype(np.complex64)
    return s, a


def main(out_path="symsync_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rx, a = _signal(seed=7)

    # per-symbol trace of the tracked rate
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    syms, rates = [], []
    for i in range(0, len(rx), SPS):
        y = s.steps(rx[i : i + SPS])
        for v in y:
            syms.append(v)
        rates.append(s.rate)
    syms = np.array(syms)

    # ambiguity-tolerant BER on the locked tail (cross-correlation aligned)
    dec = np.where(syms.real >= 0, 1, -1).astype(float)
    c = np.correlate(dec, a.astype(float), "full")
    kk = int(np.argmax(np.abs(c)))
    lag = kk - (len(a) - 1)
    inv = int(np.sign(c[kk]))
    lo, hi = len(dec) // 4, len(dec) - len(dec) // 4
    err = cnt = 0
    for i in range(lo, hi):
        j = i - lag
        if 0 <= j < len(a):
            err += dec[i] != inv * a[j]
            cnt += 1
    ber = err / cnt

    fig, (a1, a2, a3) = plt.subplots(3, 1, figsize=(9, 9))

    t = np.arange(len(syms))
    ref = np.concatenate([a, np.zeros(max(0, len(dec) - len(a)))])
    good = dec == inv * ref[np.clip(t - lag, 0, len(a) - 1)]
    a1.scatter(t[good], syms.real[good], s=4, color="#2ca02c", label="correct")
    if (~good).any():
        a1.scatter(
            t[~good], syms.real[~good], s=10, color="#d62728", label="error"
        )
    a1.axhline(0, color="0.6", lw=0.6)
    a1.set_xlabel("symbol index")
    a1.set_ylabel("recovered Re")
    a1.set_title(
        f"Gardner timing recovery — clean symbols, amb-BER = {ber:.3g}",
        fontsize=10,
    )
    a1.legend(fontsize=8, loc="center right")
    a1.grid(alpha=0.3)

    a2.axhline(
        SPS * CLOCK_RATE,
        color="k",
        ls="--",
        lw=1.3,
        label=f"true clock ({SPS * CLOCK_RATE:.3f})",
    )
    a2.plot(
        np.linspace(0, len(syms), len(rates)),
        rates,
        color="#1f77b4",
        lw=1.1,
        label="tracked samples/symbol",
    )
    a2.set_xlabel("symbol index")
    a2.set_ylabel("samples / symbol")
    a2.set_ylim(SPS - 0.1, SPS + 0.1)
    a2.set_title(
        "Tracking the asynchronous (0.4% fast) symbol clock", fontsize=10
    )
    a2.legend(fontsize=8, loc="upper right")
    a2.grid(alpha=0.3)

    # eye diagram: overlay the oversampled input folded to two symbols
    tail = rx[len(rx) // 2 :].real
    span = 2 * SPS
    ntr = len(tail) // span
    eye = tail[: ntr * span].reshape(ntr, span)
    for row in eye[:400]:
        a3.plot(np.arange(span), row, color="#1f77b4", lw=0.3, alpha=0.15)
    a3.set_xlabel("sample within 2 symbols")
    a3.set_ylabel("amplitude")
    a3.set_title(
        "Input eye (oversampled) — open, but the optimal instant drifts "
        "with the async clock",
        fontsize=10,
    )
    a3.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}  (amb-BER={ber:.3g})")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "symsync_demo.png")
