"""mpsk_nda_theory_demo.py — the NDA M-th-power carrier loop vs theory.

Two views of :class:`doppler.track.CarrierNda`, the non-data-aided loop:

  * **Discriminator S-curve** — `phase_error(φ)` is the scaled M-th-power
    detector `Im(z^M)·{1, ½, ¼}` for M = 2, 4, 8: a sawtooth of period `2π/M`
    with **slope 2 at lock for every M** (the gain is M-normalized, so one loop
    `bn` behaves the same across BPSK/QPSK/8PSK).

  * **Cold-start frequency acquisition** — the loop pulls a carrier frequency
    step onto the truth (black dashed) with **no data and no symbol timing**:
    here on a bare *unmodulated* carrier. The M-th power strips any modulation,
    so it also locks modulated data before timing is set up (see the tests).

Run:  python -m doppler.examples.mpsk_nda_theory_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:track]
import numpy as np

from doppler.mpsk import mpsk_map
from doppler.track import CarrierNda

# A QPSK signal at 8 samples/symbol carrying a residual carrier offset.
F0 = 0.0015  # residual carrier, cycles/sample
rng = np.random.default_rng(0)
labels = rng.integers(0, 4, 2000).astype(np.uint8)
tx = np.repeat(mpsk_map(labels, 4), 8).astype(np.complex64)
k = np.arange(tx.size)
rx = (tx * np.exp(2j * np.pi * F0 * k)).astype(np.complex64)

# QPSK NDA loop: 8 samples/symbol, sps/n = 2-sample boxcar arm; cold
# start — no data aiding and no symbol timing needed to lock.
c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=8, n=4, m=4)
derot = c.steps(rx)  # de-rotated samples (one per input sample)
f_est = c.norm_freq  # tracked carrier (cycles/sample)
locked = c.lock  # M-th-power lock metric (normalised: -> ~1 when locked)
# --8<-- [end:track]

# The narrative run above must acquire on modulated data with no symbol
# timing — the data-blind M-th power is exactly what buys that.
assert abs(f_est - F0) < 1e-4, "QPSK NDA narrative run failed to acquire"

ORDERS = [
    (2, "BPSK", "#1f77b4"),
    (4, "QPSK", "#2ca02c"),
    (8, "8PSK", "#d62728"),
]
SPS, N = 8, 4
ARM = SPS // N

# The acquisition panel's loop, as a SPEC: 0.01 cycles/sample, zeta 0.707.
# 911c9d01 (#300) redefined CarrierNda's bn from "per arm update" to
# cycles/sample. This demo used to pass bn=0.02, which at ARM = 2 samples per
# update WAS 0.01 cycles/sample. After #300 the same literal built a loop
# twice as wide and ~3x as jittery, and nothing noticed (#1675). The jitter
# assert in main() is derived from this spec, so the loop _acquire builds
# must be this loop.
BN_SPEC, ZETA = 0.01, 0.707
SIGMA = 0.05  # per-component noise std on the unit-amplitude carrier


def _disc(m, phi):
    # near-zero bn + NCO at 0 => identity wipe-off; one arm dump => last_error
    c = CarrierNda(bn=1e-9, zeta=0.707, init_norm_freq=0.0, sps=SPS, n=N, m=m)
    c.steps(np.full(ARM, np.exp(1j * phi), dtype=np.complex64))
    return c.last_error


def _acquire(m, f0, nsym=1200, seed=0, sigma=SIGMA):
    rng = np.random.default_rng(seed)
    k = np.arange(nsym * SPS)
    rx = np.exp(2j * np.pi * f0 * k)  # unmodulated carrier (no data)
    rx = rx + sigma * (
        rng.standard_normal(k.size) + 1j * rng.standard_normal(k.size)
    )
    rx = rx.astype(np.complex64)
    c = CarrierNda(
        bn=BN_SPEC, zeta=ZETA, init_norm_freq=0.0, sps=SPS, n=N, m=m
    )
    freq = np.empty(nsym)
    for s in range(nsym):
        c.steps(rx[s * SPS : (s + 1) * SPS])
        freq[s] = c.norm_freq
    return freq


def _jitter_bound(kd):
    """Tail frequency-jitter bound (cycles/sample) for the BN_SPEC loop.

    The linearised type-2 loop, driven by white phase noise of variance
    ``SIGMA**2`` rad^2 per sample (a unit carrier with per-component noise
    ``SIGMA``), leaves its integrator -- the tracked frequency -- with

        sigma_f = SIGMA / (2 pi) * sqrt(Kd * wn**3 / (4 zeta))

    where Kd is the discriminator's slope at lock (the S-curve's, measured
    by the caller) and wn the loop's natural frequency. wn is not restated
    here: it is read back from the library's own PI gains, which for the
    bilinear design satisfy ki/kp = wn*t/(2 zeta), at t = 1 (this loop
    updates every sample). A step-by-step simulation of the discrete loop
    (the 2-sample boxcar arm included) agrees with this closed form to 1%.
    The loop measures ~8% above it, because the model is linear and leaves
    out the M-th power's higher-order noise terms and the arm AGC, and a
    600-symbol std is itself uncertain by several percent. So the bound is
    1.5x the theory: loose enough for both, and well below the ~2.8x that
    doubling the bandwidth costs (sigma_f grows as bn**1.5).
    """
    from doppler.track import LoopFilter

    lf = LoopFilter(BN_SPEC, ZETA, 1.0)
    wn = 2.0 * ZETA * lf.ki / lf.kp
    theory = SIGMA / (2 * np.pi) * np.sqrt(kd * wn**3 / (4 * ZETA))
    return 1.5 * theory, theory


def main(out_path="mpsk_nda_theory_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (a, b) = plt.subplots(1, 2, figsize=(11, 4.5))

    max_err = 0.0
    phis = np.linspace(-np.pi, np.pi, 721)
    for m, name, col in ORDERS:
        seg = 2 * np.pi / m
        meas = np.array([_disc(m, p) for p in phis])
        scale = {2: 1.0, 4: 0.5, 8: 0.25}[m]
        theory = scale * np.sin(m * phis)
        wrapped = (phis + seg / 2) % seg - seg / 2
        guard = 3 * (phis[1] - phis[0])
        ok = np.abs(np.abs(wrapped) - seg / 2) > guard
        max_err = max(max_err, np.max(np.abs(meas[ok] - theory[ok])))
        a.plot(
            np.degrees(phis), meas, color=col, lw=1.4, label=f"{name} (M={m})"
        )
    a.axhline(0, color="0.7", lw=0.6)
    a.set_xlabel("phase error φ (deg)")
    a.set_ylabel("phase_error = Im(z^M)·scale")
    a.set_title("NDA M-th-power S-curve (sawtooth 2π/M, slope 2)", fontsize=10)
    a.set_xticks([-180, -90, 0, 90, 180])
    a.legend(fontsize=8, loc="upper right")
    a.grid(alpha=0.3)

    f0 = 0.0015
    for m, name, col in ORDERS:
        freq = _acquire(m, f0)
        b.plot(freq, color=col, lw=1.2, label=f"{name} (M={m})")
        # Cold-start acquisition must succeed for every M: the last-300-
        # symbol mean of the tracked frequency sits on the injected step.
        tail_err = abs(float(np.mean(freq[-300:])) - f0)
        print(f"{name}: tail freq err {tail_err:.2e} cycles/sample")
        assert tail_err < 1e-4, f"{name} failed to acquire the carrier"
        # And it must sit on it with the jitter of the BN_SPEC loop: a
        # loop built wider than the spec (bn=0.02 is ~2.8x) fails here.
        kd = (_disc(m, 1e-3) - _disc(m, -1e-3)) / 2e-3  # slope at lock
        bound, theory = _jitter_bound(kd)
        jitter = float(np.std(freq[-600:]))
        print(
            f"{name}: tail freq jitter {jitter:.2e} (theory {theory:.2e}, "
            f"bound {bound:.2e}) cycles/sample"
        )
        assert jitter < bound, f"{name} tracks with more jitter than BN_SPEC"
    b.axhline(f0, color="k", ls="--", lw=1.5, label=f"true f0 = {f0}")
    b.set_xlabel("symbol index")
    b.set_ylabel("tracked freq (cycles/sample)")
    b.set_title("Cold-start acquisition — no data, no timing", fontsize=10)
    b.legend(fontsize=8, loc="lower right")
    b.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}  (S-curve max err vs theory {max_err:.2e})")

    # ── self-validation ───────────────────────────────────────────────────
    # Off the wrap boundaries the noiseless M-th-power detector must trace
    # scale·sin(Mφ) to float32 round-off — the M-normalized gain is what
    # makes one loop bn behave identically across BPSK/QPSK/8PSK.
    assert max_err < 1e-5, "S-curve departs from scale*sin(Mφ)"


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "mpsk_nda_theory_demo.png")
