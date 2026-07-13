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

import numpy as np

from doppler.track import CarrierNda

ORDERS = [
    (2, "BPSK", "#1f77b4"),
    (4, "QPSK", "#2ca02c"),
    (8, "8PSK", "#d62728"),
]
SPS, N = 8, 4
ARM = SPS // N


def _disc(m, phi):
    # near-zero bn + NCO at 0 => identity wipe-off; one arm dump => last_error
    c = CarrierNda(bn=1e-9, zeta=0.707, init_norm_freq=0.0, sps=SPS, n=N, m=m)
    c.steps(np.full(ARM, np.exp(1j * phi), dtype=np.complex64))
    return c.last_error


def _acquire(m, f0, nsym=1200, seed=0, sigma=0.05):
    rng = np.random.default_rng(seed)
    k = np.arange(nsym * SPS)
    rx = np.exp(2j * np.pi * f0 * k)  # unmodulated carrier (no data)
    rx = rx + sigma * (
        rng.standard_normal(k.size) + 1j * rng.standard_normal(k.size)
    )
    rx = rx.astype(np.complex64)
    c = CarrierNda(bn=0.02, zeta=0.707, init_norm_freq=0.0, sps=SPS, n=N, m=m)
    freq = np.empty(nsym)
    for s in range(nsym):
        c.steps(rx[s * SPS : (s + 1) * SPS])
        freq[s] = c.norm_freq
    return freq


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
