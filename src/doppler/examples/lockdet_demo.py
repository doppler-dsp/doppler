"""lockdet_demo.py — chatter-free lock flags from probability budgets.

Drives :class:`doppler.detection.LockDet` with a per-look envelope
statistic through a signal-off -> marginal-signal -> signal-off
sequence, side by side with the naive single-comparison flag
(``metric > threshold``) every loop starts life with. At a marginal
per-look detection probability the naive flag chatters on every miss;
the lock detector's consecutive-look verify counts and threshold band
compound the same per-look probabilities into a flag that transitions
exactly twice — once up, once down — with a declare latency that is
*predicted*, not tuned.

Every constant is derived from a probability budget:

* per-look threshold from the per-look false-alarm rate
  (:func:`det_threshold`),
* the declare count from a compound false-declare budget
  (:func:`det_verify_count` — consecutive looks compound as ``p^n``),
* the declare latency check from the run-length waiting time
  (:func:`det_verify_delay`), verified against a Monte-Carlo histogram.

Three views (saved to a PNG):
  * **Per-look statistic** — Rayleigh (noise) and Rician (signal)
    envelope draws crossing the declare/drop threshold band.
  * **The two flags** — the naive comparator's dozens of transitions
    vs the lock detector's two.
  * **Declare latency** — Monte-Carlo looks-to-declare distribution
    against the det_verify_delay() mean.

Run:  python -m doppler.examples.lockdet_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.detection import (
    LockDet,
    det_threshold,
    det_verify_count,
    det_verify_delay,
)

PFA_LOOK = 1e-2  # per-look false-alarm probability
DECLARE_BUDGET = 1e-6  # compound false-declare budget
NU = 4.0  # Rician non-centrality: a marginal signal
N_DOWN = 10  # drop-side verify count (see the gallery page)
DOWN_FRAC = 0.8  # drop threshold as a fraction of eta
SEGS = (200, 300, 200)  # looks: noise | signal | noise
N_MC = 4000  # declare-latency Monte-Carlo trials


def _looks(rng, n, nu=0.0):
    """n per-look envelope draws: Rayleigh (nu=0) or Rician(nu, 1)."""
    re = rng.normal(nu, 1.0, n)
    im = rng.normal(0.0, 1.0, n)
    return np.sqrt(re**2 + im**2)


def main(out_path="lockdet_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rng = np.random.default_rng(7)

    # ── derived decision rule ────────────────────────────────────────
    eta = det_threshold(PFA_LOOK)
    n_up = det_verify_count(PFA_LOOK, DECLARE_BUDGET)
    down = DOWN_FRAC * eta

    # ── one on/off/on trace ──────────────────────────────────────────
    n0, n1, n2 = SEGS
    x = np.concatenate(
        [
            _looks(rng, n0),
            _looks(rng, n1, nu=NU),
            _looks(rng, n2),
        ]
    )
    naive = (x > eta).astype(int)
    d = LockDet(up_thresh=eta, down_thresh=down, n_up=n_up, n_down=N_DOWN)
    flag = d.steps(x)

    t_naive = int(np.count_nonzero(np.diff(naive)))
    t_lock = int(np.count_nonzero(np.diff(flag)))
    print(f"threshold eta = {eta:.3f}  (pfa/look = {PFA_LOOK:g})")
    print(f"verify counts: n_up = {n_up}, n_down = {N_DOWN}")
    print(f"naive comparator transitions: {t_naive}")
    print(f"lock detector transitions:    {t_lock}")
    assert t_lock == 2, "expected exactly one declare and one drop"

    # ── declare-latency Monte Carlo vs det_verify_delay ──────────────
    sig = _looks(rng, 200_000, nu=NU)
    pd_look = float(np.mean(sig > eta))  # empirical per-look pd
    lat = np.empty(N_MC)
    for k in range(N_MC):
        mc = LockDet(up_thresh=eta, down_thresh=down, n_up=n_up)
        run = sig[rng.integers(0, sig.size - 400) :]
        got = np.flatnonzero(mc.steps(run[:400]))
        lat[k] = got[0] + 1 if got.size else np.nan
    lat = lat[~np.isnan(lat)]
    predicted = det_verify_delay(pd_look, n_up)
    print(f"per-look pd = {pd_look:.3f}")
    print(
        f"declare latency: MC mean = {lat.mean():.2f} looks, "
        f"det_verify_delay = {predicted:.2f} looks"
    )

    # ── figure ───────────────────────────────────────────────────────
    fig, (ax0, ax1, ax2) = plt.subplots(
        3,
        1,
        figsize=(9.6, 8.2),
        gridspec_kw={"height_ratios": [3.0, 1.2, 2.2]},
    )
    look = np.arange(x.size)

    ax0.plot(look, x, lw=0.6, color="#607d8b", label="per-look statistic")
    ax0.axhspan(
        down, eta, color="#ffb300", alpha=0.25, label="hysteresis band"
    )
    ax0.axhline(eta, color="#e65100", lw=1.2, label=r"declare $\eta$")
    ax0.axhline(down, color="#e65100", lw=1.2, ls="--", label="drop 0.8η")
    ax0.axvspan(n0, n0 + n1, color="#4caf50", alpha=0.10)
    ax0.text(
        n0 + n1 / 2,
        ax0.get_ylim()[1] * 0.94,
        "signal present",
        ha="center",
        color="#2e7d32",
    )
    ax0.set_ylabel("envelope")
    ax0.legend(loc="upper left", fontsize=8, ncol=2)
    ax0.set_title(
        "Lock detection: verify counts + hysteresis on a marginal signal"
    )

    ax1.step(look, naive + 1.4, where="post", color="#b71c1c", lw=0.8)
    ax1.step(look, flag, where="post", color="#1565c0", lw=1.4)
    ax1.set_yticks([0.5, 1.9])
    ax1.set_yticklabels(
        [f"LockDet ({t_lock} transitions)", f"naive (> η) ({t_naive})"]
    )
    ax1.set_xlabel("look")
    ax1.set_ylim(-0.3, 2.7)

    ax2.hist(
        lat,
        bins=np.arange(0.5, 25.5),
        density=True,
        color="#90a4ae",
        label="Monte-Carlo looks-to-declare",
    )
    ax2.axvline(
        predicted,
        color="#e65100",
        lw=1.6,
        label=f"det_verify_delay(pd={pd_look:.2f}, n={n_up}) "
        f"= {predicted:.2f}",
    )
    ax2.axvline(
        lat.mean(),
        color="#1565c0",
        lw=1.6,
        ls="--",
        label=f"MC mean = {lat.mean():.2f}",
    )
    ax2.set_xlabel("looks to declare")
    ax2.set_ylabel("density")
    ax2.legend(fontsize=8)

    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(*sys.argv[1:2])
