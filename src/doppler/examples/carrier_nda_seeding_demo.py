"""carrier_nda_seeding_demo.py — sizing a coarse acquisition for the NDA loop.

:class:`doppler.track.CarrierNda` is meant to be **handed** a residual carrier,
not to find one by grinding. This demo is the handoff: a coarse frequency
estimate seeds the loop, and the loop locks promptly because the seed landed
inside a window whose size the loop itself dictates.

The rule, measured across three constellation orders and three loop
bandwidths in ``src/doppler/track/tests/validation/carrier_nda`` §2.5:

    seed within  |Δf| ≤ bn / M   →   settled within  2 / bn  samples

and it inverts into the number that sizes the search: **the acquisition's
frequency bin must be no wider than ``2·bn/M``**, so that whichever bin wins
leaves the residual inside ±``bn/M``.

Why that window and not a looser one: inside it the settling time does not
depend on the offset at all — a linear second-order loop settles in a fixed
number of time constants whatever the size of the step — so acquisition is
*constant time*, not merely bounded. Outside it the loop still pulls in, but
via a beat-note grind whose cost is quadratic in the offset and which noise
disrupts. This demo shows both, on the same signal.

The coarse estimate is the classical NDA one and uses the same trick the loop
does: raise the limited signal to the Mth power, which strips the M-PSK data,
and read the tone off an FFT. That leaves the M-fold FREQUENCY ambiguity —
``Δf`` and ``Δf ± k/M`` are indistinguishable to an M-th-power detector — so
the search range must be bounded to pick a branch. Here it is |Δf| < 1/(2M),
and the candidate nearest DC is taken. See ``docs/design/mpsk.md`` §3.5.

Run:  python -m doppler.examples.carrier_nda_seeding_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:seed]
import numpy as np

from doppler.mpsk import mpsk_map
from doppler.spectral import FFT
from doppler.telemetry import Telemetry
from doppler.track import CarrierNda
from doppler.wfm import PN, Synth

SPS, M, BN, N_ARM = 8, 4, 0.01, 4
#: The acquisition's bin, sized by the loop: no wider than 2*bn/M, so the
#: residual after seeding is at most bn/M whichever bin wins.
BIN = 2 * BN / M
#: A true carrier sitting half a bin off the nearest bin centre — the WORST
#: case the rule has to cover, not a convenient one.
F0 = 0.0225
NFFT = 8192


def mth_power_spectrum(rx: np.ndarray, m: int, nfft: int) -> np.ndarray:
    """Block-averaged periodogram of the limited signal's Mth power.

    Averaged over as many whole blocks as the record holds, because ONE
    block is not a detector at this SNR: the Mth power multiplies the
    noise-induced phase excursion by M, and a single 8192-point transform
    leaves the carrier only ~3 dB above the floor. Non-coherent
    integration across blocks is what a real acquisition does, and it is
    the difference between a peak you can threshold and one you can only
    find because you already know where it is.
    """
    nblk = max(1, rx.size // nfft)
    fft = FFT(n=nfft, sign=-1)
    acc = np.zeros(nfft)
    for b in range(nblk):
        seg = rx[b * nfft : (b + 1) * nfft].astype(np.complex128)
        unit = seg / np.maximum(np.abs(seg), 1e-12)
        spec = np.asarray(fft.execute_cf32((unit**m).astype(np.complex64)))
        acc += np.abs(spec) ** 2
    return acc / nblk


def coarse_estimate(rx: np.ndarray, m: int, nfft: int) -> float:
    """Classical NDA coarse frequency estimate, in cycles/sample.

    Limit, raise to the Mth power (which strips the data, exactly as the
    loop's discriminator does), and read the peak bin. Dividing by M leaves
    M candidates spaced 1/M apart — the frequency half of the M-fold
    ambiguity — so this returns the one nearest DC, which is correct only
    because the search is bounded to |Δf| < 1/(2M).
    """
    peak = int(np.argmax(mth_power_spectrum(rx, m, nfft)))
    f_mth = peak / nfft
    if f_mth > 0.5:  # the FFT's bins run 0..1; fold to ±1/2
        f_mth -= 1.0
    cands = f_mth / m + np.arange(m) / m
    cands = np.where(cands > 0.5, cands - 1.0, cands)
    return float(cands[int(np.argmin(np.abs(cands)))])


def track(rx: np.ndarray, seed_hz: float) -> tuple[np.ndarray, CarrierNda]:
    """Run one loop from a given seed; return its frequency track."""
    loop = CarrierNda(
        bn=BN, zeta=0.707, init_norm_freq=seed_hz, sps=SPS, n=N_ARM, m=M
    )
    tlm = Telemetry(1 << 20)
    loop.set_telemetry(tlm, "car", 8)
    loop.steps(rx)
    return np.asarray(tlm.read_dict()["car.freq"]), loop


# --8<-- [end:seed]


#: Probe decimation above, so a settling index is in units of 8 samples.
DECIM = 8


def settled_after(freq: np.ndarray, truth: float, tol: float = 0.05) -> int:
    """Samples until the tracked frequency stays within `tol` of `truth`.

    The LAST excursion out of the band, not the first arrival into it: a
    loop crossing its target on the way past has not settled, and this one
    overshoots (zeta = 0.707). Same convention as the validation report and
    as `test_carrier_nda_core.c` section 16, so the three numbers are
    comparable.
    """
    out = np.flatnonzero(np.abs(freq - truth) > tol * abs(truth))
    return (int(out[-1]) + 1) * DECIM if out.size else 0


def qpsk_at(f0: float, nsym: int = 6000, esno_db: float = 20.0) -> np.ndarray:
    """Random QPSK on a carrier at `f0`, with NO symbol-timing alignment.

    Generated by `Synth` with labels from doppler's own `PN`, so the
    stimulus is the library's rather than a second waveform to keep in
    agreement by hand.
    """
    bits = np.asarray(PN(poly=0, seed=2, length=15).generate(nsym * 2))
    pairs = bits[: nsym * 2].astype(np.uint8).reshape(nsym, 2)
    labels = (pairs[:, 0] * 2 + pairs[:, 1]).astype(np.uint8)
    return np.asarray(
        Synth(
            type="symbols",
            symbols=mpsk_map(labels, M),
            sps=SPS,
            pulse="rect",
            freq=f0,
            fs=1.0,
            snr=esno_db,
            snr_mode="esno",
            seed=7,
        ).steps(nsym * SPS)
    ).astype(np.complex64)


def main(out_path: str) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rx = qpsk_at(F0)

    # ── the handoff ───────────────────────────────────────────────────────
    f_fine = coarse_estimate(rx, M, NFFT)
    f_seed = round(f_fine / BIN) * BIN  # what a binned search reports
    residual = abs(F0 - f_seed)
    window = BN / M
    budget = 2.0 / BN

    cold_track, cold = track(rx, 0.0)
    seed_track, warm = track(rx, f_seed)
    s_cold = settled_after(cold_track, F0)
    s_seed = settled_after(seed_track, F0)

    print(f"true carrier      {F0:.5f} cycles/sample")
    print(f"coarse estimate   {f_fine:.5f}  (M-th power + {NFFT}-pt FFT)")
    print(f"seeded at         {f_seed:.5f}  (quantised to a {BIN:g} bin)")
    print(f"residual          {residual:.5f}  against a window of {window:g}")
    print(f"unseeded settled  {s_cold:6d} samples = {s_cold * BN:7.2f} / bn")
    print(f"seeded settled    {s_seed:6d} samples = {s_seed * BN:7.2f} / bn")
    print(f"the rule          settle within 2 / bn = {budget:.0f} samples")

    # ── figure ────────────────────────────────────────────────────────────
    fig, (a, b) = plt.subplots(1, 2, figsize=(13, 4.6))

    spec = np.sqrt(mth_power_spectrum(rx, M, NFFT))
    fax = np.arange(NFFT) / NFFT
    fax = np.where(fax > 0.5, fax - 1.0, fax)
    order = np.argsort(fax)
    a.plot(fax[order], 20 * np.log10(spec[order] + 1e-9), lw=0.8)
    a.axvline(M * F0, color="k", ls="--", lw=1.2, label=f"M·Δf = {M * F0:.3f}")
    a.set_xlim(-0.5, 0.5)
    a.set_xlabel("frequency of the M-th power (cycles/sample)")
    a.set_ylabel("magnitude (dB)")
    a.set_title(
        f"Coarse estimate: the M-th power strips the data,\n"
        f"leaving one tone at M·Δf ({rx.size // NFFT} blocks averaged)",
        fontsize=10,
    )
    a.legend(fontsize=8)
    a.grid(alpha=0.3)

    t = np.arange(cold_track.size) * DECIM
    b.plot(t, cold_track, lw=1.1, color="tab:red", label="cold start (Δf = 0)")
    b.plot(
        t, seed_track, lw=1.4, color="tab:green", label=f"seeded at {f_seed:g}"
    )
    b.axhline(F0, color="k", ls="--", lw=1.2, label=f"true Δf = {F0}")
    b.axhspan(
        F0 - window,
        F0 + window,
        color="tab:green",
        alpha=0.12,
        label=f"±bn/M = ±{window:g}",
    )
    b.axvline(
        budget,
        color="0.35",
        ls=":",
        lw=1.4,
        label=f"2/bn = {budget:.0f} samples",
    )
    b.set_xscale("symlog", linthresh=100)
    b.set_xlim(0, t[-1])
    b.set_xlabel("sample")
    b.set_ylabel("tracked carrier (cycles/sample)")
    b.set_title(
        "Seeded inside ±bn/M: locked in "
        f"{s_seed * BN:.1f}/bn against {s_cold * BN:.0f}/bn cold",
        fontsize=10,
    )
    b.legend(fontsize=8, loc="lower right")
    b.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")

    # ── self-validation ───────────────────────────────────────────────────
    # 1. The premise: a bin of 2*bn/M really does leave the residual inside
    #    the window, for a carrier placed at the worst point in its bin.
    assert residual <= window * 1.01, (
        f"seed residual {residual:.5f} exceeds the window {window:g} — the "
        f"bin is too wide or the coarse estimate is off"
    )
    # 2. The rule itself. Asserted at 2.5/bn rather than the published 2/bn
    #    for platform margin, exactly as test_carrier_nda_core.c section 16
    #    does; the measured value is printed above and is ~1.4/bn here.
    assert s_seed <= 2.5 * budget / 2.0, (
        f"seeded loop took {s_seed * BN:.2f}/bn, outside the 2/bn rule"
    )
    # 3. And the contrast that makes seeding worth doing at all. Without it
    #    this demo would pass on a loop that locked instantly from anywhere,
    #    which is the vacuous version of the same story.
    assert s_cold > 10 * s_seed, (
        f"cold start ({s_cold * BN:.1f}/bn) is not materially slower than "
        f"seeded ({s_seed * BN:.1f}/bn) — the regimes should differ by "
        f"orders"
    )
    # 4. Both runs must reach the SAME carrier: a seed that biased the
    #    answer would be worse than a slow lock.
    assert abs(cold.norm_freq - warm.norm_freq) < 1e-4, (
        "seeded and cold-start runs converged on different carriers"
    )
    assert abs(warm.norm_freq - F0) < 5e-4, "seeded run did not acquire"
    assert warm.locked, "seeded run never declared lock"


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "carrier_nda_seeding_demo.png")
