"""dll_demo.py — code-loop tracking vs time.

Drives :class:`doppler.track.Dll` with a continuous PN-spread BPSK signal whose
code arrives with an initial **half-chip offset** and a slow **code Doppler**
(the chip clock running slightly fast). The delay-lock loop pulls its early /
prompt / late replica onto the incoming code and then tracks the drift.

Two views (saved to a PNG):
  * **Code-rate tracking** — the loop's chip-rate estimate (blue) ringing in
    and settling onto the true incoming rate (black dashed).
  * **Loop stress vs time** — sliding-RMS of the non-coherent E-minus-L
    discriminator: a pull-in transient that decays to a low locked floor.

Run:  python -m doppler.examples.dll_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:loop]
import numpy as np

from doppler.track import Dll

SF, SPS = 127, 8  # code length (chips), samples per chip
NPER = 1000  # code periods
OFFSET = 0.5  # initial replica offset, chips
DELTA = 2e-4  # code Doppler (chip rate error)
BN = 0.004  # loop noise bandwidth

# code: 0/1 chips for one period, and its +1/-1 spreading signs.
code = np.random.default_rng(1).integers(0, 2, SF).astype(np.uint8)
csign = np.where(code & 1, -1.0, 1.0)

# Carrier-wiped PN-spread BPSK whose chip clock runs (1 + DELTA) fast,
# with one random BPSK data sign per code period.
rng = np.random.default_rng(9)
data = rng.integers(0, 2, NPER) * 2 - 1
cph = np.arange(SF * SPS * NPER) * (1 + DELTA) / SPS  # running chip phase
rx = (np.repeat(data, SF * SPS) * csign[(cph % SF).astype(int)]).astype(
    np.complex64
)

# The replica starts OFFSET chips off; the loop pulls in, then tracks.
d = Dll(code, sps=SPS, init_chip=OFFSET, bn=BN, zeta=0.707, spacing=0.5)
symbols = d.steps(rx)  # one prompt symbol per code period
rate = d.code_rate  # tracked chip rate (1.0 + code Doppler)
phase = d.code_phase  # tracked code phase (chips)
# --8<-- [end:loop]

# The narrative run above must land on the true incoming chip rate —
# the same physics the per-period sweep asserts in detail in main().
assert abs(rate - (1.0 + DELTA)) < 0.1 * DELTA, "DLL missed the true rate"
assert d.locked, "DLL lock detector never declared lock"


def main(out_path="dll_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # A fresh loop over the same signal, sampled once per code period so
    # the pull-in transient and the locked floor can be plotted.
    d = Dll(code, sps=SPS, init_chip=OFFSET, bn=BN, zeta=0.707, spacing=0.5)

    n_per = SF * SPS
    rate = np.empty(NPER)
    stress = np.empty(NPER)
    for p in range(NPER):
        d.steps(rx[p * n_per : (p + 1) * n_per])
        rate[p] = d.code_rate
        stress[p] = d.last_error

    # ── self-validation: the demo's physics, asserted ────────────────────
    # After pull-in the loop must sit on the true incoming chip rate; the
    # tail error is a small fraction of the injected code Doppler itself.
    tail = slice(NPER - 200, None)
    rate_err = abs(float(np.mean(rate[tail])) - (1.0 + DELTA))
    print(f"tail code-rate error {rate_err:.2e} (code Doppler {DELTA:.0e})")
    assert rate_err < 0.1 * DELTA, "DLL did not settle on the true rate"
    assert d.locked, "DLL lock detector never declared lock"
    # The half-chip pull-in must show as a large early E-minus-L swing
    # that decays to a low locked discriminator floor.
    head_rms = float(np.sqrt(np.mean(stress[:50] ** 2)))
    tail_rms = float(np.sqrt(np.mean(stress[tail] ** 2)))
    print(f"discriminator RMS: pull-in {head_rms:.3f} -> floor {tail_rms:.3f}")
    assert head_rms > 0.3, "no pull-in transient — replica already aligned?"
    assert tail_rms < 0.12, "locked discriminator floor too high"

    t = np.arange(NPER)
    fig, (a, b) = plt.subplots(2, 1, figsize=(9, 6), sharex=True)

    a.axhline(1.0 + DELTA, color="k", ls="--", lw=1.4, label="true rate")
    a.plot(t, rate, color="#1f77b4", lw=1.1, label="DLL estimate")
    a.set_ylabel("code rate (chips/chip)")
    a.set_title(
        f"DLL: pull in a {OFFSET:.1f}-chip offset, track a "
        f"{DELTA:.0e} code Doppler (SF={SF}, sps={SPS})",
        fontsize=10,
    )
    a.legend(fontsize=8, loc="upper right")
    a.grid(alpha=0.3)

    def rms(x, w=40):
        k = np.ones(w) / w
        return np.sqrt(np.convolve(x * x, k, mode="same"))

    b.plot(t, rms(stress), color="#d62728", lw=1.2)
    b.set_ylabel("RMS discriminator")
    b.set_xlabel("code period")
    b.set_title(
        "Loop stress vs time — E-minus-L pull-in transient decays to a "
        "locked floor",
        fontsize=10,
    )
    b.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "dll_demo.png")
