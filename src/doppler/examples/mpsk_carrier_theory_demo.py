"""mpsk_carrier_theory_demo.py — M-PSK carrier loop validated against theory.

Two theoretical-correctness views of :class:`doppler.track.CarrierMpsk`'s
decision-directed M-PSK discriminator ``e = Im(P·conj(â))/|P|`` (â the nearest
constellation point), for M = 2 (BPSK), 4 (QPSK), 8 (8PSK):

  * **Phase-detector S-curve** — drive the loop open-loop (bandwidth → 0)
    with a noiseless constellation point rotated by a swept static phase
    error φ and read the discriminator. While the decision is correct
    (|φ| < π/M) the nearest point is the transmitted one, so ``e = sin(φ)``
    — a sine through the origin with **unit slope**. As φ crosses ±π/M the
    slicer jumps to the
    adjacent point and ``e`` snaps to ``sin(φ ∓ 2π/M)``: a **sawtooth of period
    2π/M**, the signature of the M-fold phase ambiguity. Peak ``±sin(π/M)``. At
    M = 2 this is exactly the BPSK Costas S-curve.

  * **Frequency acquisition** — a residual carrier step (cycles/sample) with
    the FLL assist enabled: the integer-NCO frequency estimate per symbol
    converges onto the true offset (black dashed). 8PSK's narrow ±π/8
    discriminator needs the FLL's wide cross-product to pull in; without it
    the bare PLL would stall.

Run:  python -m doppler.examples.mpsk_carrier_theory_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:track]
import numpy as np

from doppler.mpsk import mpsk_map
from doppler.track import CarrierMpsk

# A QPSK signal at 16 samples/symbol carrying a residual carrier offset.
F0 = 0.0015  # residual carrier, cycles/sample
rng = np.random.default_rng(0)
labels = rng.integers(0, 4, 1500).astype(np.uint8)
tx = np.repeat(mpsk_map(labels, 4), 16).astype(np.complex64)
k = np.arange(tx.size)
rx = (tx * np.exp(2j * np.pi * F0 * k)).astype(np.complex64)

# QPSK carrier loop, 16 samples/symbol, FLL-assisted.
c = CarrierMpsk(
    bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=16, bn_fll=0.01, m=4
)
symbols = c.steps(rx)  # one complex prompt per symbol
f_est = c.norm_freq  # tracked residual carrier (cycles/sample)
locked = c.lock_metric  # Re(P·conj â)/|P| EMA, ~1.0 when phase-locked
# --8<-- [end:track]

# The narrative run above must acquire the offset — the same check the
# per-M acquisition sweep in main() makes on its tail.
assert abs(f_est - F0) < 1e-4, "QPSK narrative run failed to acquire"

ORDERS = [
    (2, "BPSK", "#1f77b4"),
    (4, "QPSK", "#2ca02c"),
    (8, "8PSK", "#d62728"),
]


def _scurve(m, phis):
    """Open-loop discriminator e(φ), averaged over the M points (symmetric)."""
    pts = mpsk_map(np.arange(m, dtype=np.uint8), m)  # constellation points
    meas = np.empty(len(phis))
    for i, phi in enumerate(phis):
        acc = 0.0
        rot = np.exp(1j * phi)
        for a in pts:
            c = CarrierMpsk(
                bn=1e-6,
                zeta=0.707,
                init_norm_freq=0.0,
                tsamps=1,
                bn_fll=0.0,
                m=m,
            )
            c.steps(np.array([a * rot], np.complex64))
            acc += c.last_error
        meas[i] = acc / m
    return meas


def _acquire(m, f0, nsym=1500, tsamps=16, seed=0):
    """Per-symbol tracked frequency while acquiring a carrier step f0."""
    rng = np.random.default_rng(seed)
    labels = rng.integers(0, m, nsym).astype(np.uint8)
    sig = np.repeat(mpsk_map(labels, m), tsamps).astype(np.complex64)
    k = np.arange(sig.size)
    rx = (sig * np.exp(2j * np.pi * f0 * k)).astype(np.complex64)
    c = CarrierMpsk(
        bn=0.05,
        zeta=0.707,
        init_norm_freq=0.0,
        tsamps=tsamps,
        bn_fll=0.01,
        m=m,
    )
    freq = np.empty(nsym)
    for s in range(nsym):
        c.steps(rx[s * tsamps : (s + 1) * tsamps])
        freq[s] = c.norm_freq
    return freq


def main(out_path="mpsk_carrier_theory_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (a, b) = plt.subplots(1, 2, figsize=(11, 4.5))

    # --- S-curves per M ------------------------------------------------------
    max_err = 0.0
    for m, name, col in ORDERS:
        seg = 2 * np.pi / m
        phis = np.linspace(-np.pi, np.pi, 721)
        meas = _scurve(m, phis)
        # theory: sawtooth sin(phi wrapped into (-pi/M, pi/M]) of period 2pi/M
        wrapped = (phis + seg / 2) % seg - seg / 2
        theory = np.sin(wrapped)
        # the error is meaningful only on the linear branches; near a
        # decision boundary (|wrapped| -> pi/M) a single grid sample can land
        # either side of the slicer snap, an O(1) artefact checked apart in C.
        guard = 3 * (phis[1] - phis[0])
        ok = np.abs(np.abs(wrapped) - seg / 2) > guard
        max_err = max(max_err, np.max(np.abs(meas[ok] - theory[ok])))
        a.plot(
            np.degrees(phis), meas, color=col, lw=1.4, label=f"{name} (M={m})"
        )
    a.axhline(0, color="0.7", lw=0.6)
    a.set_xlabel("phase error φ (deg)")
    a.set_ylabel("discriminator e")
    a.set_title(
        "M-PSK S-curve: sawtooth, period 2π/M, unit slope at lock", fontsize=10
    )
    a.set_xticks([-180, -90, 0, 90, 180])
    a.legend(fontsize=8, loc="upper right")
    a.grid(alpha=0.3)

    # --- frequency acquisition per M (FLL-assisted) --------------------------
    f0 = 0.0015
    for m, name, col in ORDERS:
        freq = _acquire(m, f0)
        b.plot(freq, color=col, lw=1.2, label=f"{name} (M={m})")
        # Every order must actually acquire: the FLL-assisted loop's
        # last-300-symbol mean estimate sits on the injected offset.
        tail_err = abs(float(np.mean(freq[-300:])) - f0)
        print(f"{name}: tail freq err {tail_err:.2e} cycles/sample")
        assert tail_err < 1e-4, f"{name} failed to acquire the carrier"
    b.axhline(f0, color="k", ls="--", lw=1.5, label=f"true f0 = {f0}")
    b.set_xlabel("symbol index")
    b.set_ylabel("tracked freq (cycles/sample)")
    b.set_title("Frequency acquisition (FLL-assisted pull-in)", fontsize=10)
    b.legend(fontsize=8, loc="lower right")
    b.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}  (S-curve max err vs theory {max_err:.2e})")

    # ── self-validation ───────────────────────────────────────────────────
    # Off the slicer boundaries the noiseless discriminator must trace the
    # sawtooth sin(φ wrapped to ±π/M) to float32 round-off, for every M.
    assert max_err < 1e-5, "S-curve departs from the M-PSK sawtooth"


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "mpsk_carrier_theory_demo.png")
