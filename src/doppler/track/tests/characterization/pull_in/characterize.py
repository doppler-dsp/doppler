"""How far out can each of `MpskReceiver`'s loops ACQUIRE?

This subject exists because its answer was a constant. The carrier loop's
acquisition bound — `bn_carrier / m` cycles per symbol, the `m` because the
NDA discriminator is an M-th power — was measured once, written into three
docstrings as dated prose, and re-derived by nothing. Two findings in
`MpskReceiver`'s validation report were filed against the receiver for
behaviour that was really a test seeded past that bound, and both were
retracted (doppler#843, doppler#849). A curve anybody can re-run is what
stops that recurring.

**Tracking is not the question.** Both loops track far beyond what they can
acquire; what is unpredictable is PULL-IN, and the number a caller needs is
how far from truth a loop may be started and still converge. So every trial
seeds an offset, runs, and asks one thing: did it get there.

Two loops, two axes, and they are stated separately because their margins
differ by more than a factor of two:

- **carrier** — offset in multiples of `bn_carrier / m` cycles per SYMBOL,
  swept across every constellation order, because the whole point of the `m`
  is that the bound is order-dependent while the multiple is not.
- **timing** — a fractional sample-clock error in multiples of `bn_timing`,
  which is already dimensionless in symbols per symbol. No `m` belongs here:
  the timing discriminator is not an M-th power, and this sweep is what
  establishes that rather than assuming it.

Everything measured comes from the receiver's own telemetry through the shared
harness — `car.locked` / `car.freq` for the carrier, `sync.locked` /
`sync.rate` for the timing loop. Nothing here builds a stimulus, a level or an
estimator; `scripts/check_stimulus_sources.py` gates that, and composing is
the rule (`docs/dev/contributing/measuring-a-receiver.md`).

Run:  make characterize      (this subject, plus every other)
      python -m doppler.track.tests.characterization.pull_in.characterize
"""

from __future__ import annotations

import numpy as np

from doppler.track.tests._mpsk_rx_harness import (
    clock_offset_inside_bw,
    demod,
    freq_offset_inside_bw,
    make_signal,
    settle_floor,
)

#: The loop bandwidths every sweep here runs at. Held FIXED across the sweep
#: on purpose: the claim is about the offset in units of the bound, and a bound
#: that moved with the axis is exactly the confound doppler#843 was about.
BN = 0.01

#: Es/N0 for every trial. High enough that the loop is not noise-limited --
#: rule 1 of the loop-characterisation rules is ~20 dB of loop SNR -- so a
#: failure to acquire is about the OFFSET and not about the signal.
ESN0_DB = 20.0

#: Symbols per trial. Long enough to clear `settle_floor(BN, BN)` (2000 at
#: these bandwidths) with room for the estimate to converge afterwards.
NSYM = 4000

#: Constellation orders. The carrier bound carries `1/m`, so sweeping all
#: three is what distinguishes a bound that tracks `m` from one that does not.
ORDERS = (2, 4, 8)

#: Oversampling. Two values, because the bound is stated in cycles per SYMBOL
#: and therefore must NOT move with `sps` -- if the collapse point differs
#: between these, the units are wrong.
SPS_CHOICES = (8, 16)

#: Seeds per point. Pull-in near its boundary is a per-record outcome, so a
#: single trial reports which way one transient fell. Six is enough to see a
#: fraction move off 1.0 without making the sweep an afternoon.
SEEDS = 6

#: Carrier offsets, in multiples of `bn_carrier / m`. Spans the region tests
#: are held to (<= 1) through the measured cliff and past it, so the curve
#: shows the shoulder rather than just its endpoints.
CARRIER_MULTIPLES = (0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0)

#: Clock errors, in multiples of `bn_timing`. Finer near 1.6-2.0, which is
#: where the previous one-off measurement put the timing loop's shoulder.
TIMING_MULTIPLES = (0.5, 1.0, 1.4, 1.6, 1.8, 2.0, 2.5, 3.0)

#: How close the converged estimate must be to truth to count as acquired, as
#: a fraction of the larger of (the offset it was asked to close, the loop's
#: own bound). A loop that ends 10% of the way from truth has acquired; one
#: parked where it started has not, and one on an M-th-power alias is further
#: away still.
#:
#: **The floor at the bound is not a fudge — without it the criterion gets
#: HARDER as the question gets easier.** Ten percent of a half-bound offset at
#: M = 8 is 7.8e-6 cycles per sample, below the loop's own steady-state
#: jitter, so a trial well inside the envelope fails for being asked to hold
#: a tighter tolerance than the loop can. This sweep's first run showed
#: exactly that: 0.83 at 0.5x against 1.00 at 1.0x, a non-monotone success
#: fraction on a monotone axis — which is the same signature this whole
#: subject exists to recognise, here produced by the measurement rather than
#: by the receiver. Scaling the tolerance to the BOUND makes it one physical
#: question at every point on the axis.
CLOSED_FRAC = 0.1


def carrier_acquired(probes, seeded_offset_sym, bound_sym, sps):
    """Did the carrier loop declare lock AND actually get there?

    Both halves are required, and each catches what the other misses. `lock`
    alone is satisfied by a stable FALSE lock at `df = k*Rs/M`, where the
    constellation is stationary and every self-referenced metric reads clean
    (`docs/design/mpsk.md` section 2.1). The frequency alone is satisfied by a
    loop that wanders through the right value without ever holding it.

    `car.freq` is the NCO's own frequency in cycles per SAMPLE; the offset is
    stated in cycles per SYMBOL, so the comparison converts once, here.
    """
    if not np.any(probes["car.locked"] > 0.5):
        return False
    residual_sample = seeded_offset_sym / float(sps)
    # The loop starts `residual_sample` BELOW truth and must climb to it, so
    # what is left at the end is the distance still to go.
    left = abs(
        probes["car.freq"][-1] - probes["car.freq"][0] - residual_sample
    )
    scale = max(abs(residual_sample), abs(bound_sym) / float(sps))
    return bool(left <= CLOSED_FRAC * scale)


def timing_acquired(probes, sps, clock_offset):
    """Did the timing loop declare lock AND steer back to the true rate?

    The receiver is told a nominal `sps` scaled by `clock_offset` while the
    stimulus runs at the true one, so `sync.rate` starts wrong by exactly that
    factor and has to be steered back — the same shape as a free-running ADC
    clock.
    """
    if not np.any(probes["sync.locked"] > 0.5):
        return False
    told = float(sps) * (1.0 + clock_offset)
    left = abs(probes["sync.rate"][-1] - float(sps))
    # Same floor as the carrier: the BOUND sets the scale, not the offset, so
    # the question does not get harder as it gets easier.
    scale = max(abs(told - float(sps)), float(sps) * BN)
    return bool(left <= CLOSED_FRAC * scale)


def run_carrier(m, sps, multiple, seed):
    """One carrier trial: seed `multiple` x the bound, report acquisition."""
    offset = freq_offset_inside_bw(BN, m, multiple)
    bound = freq_offset_inside_bw(BN, m, 1.0)
    x, _idx = make_signal(
        sps, NSYM, real=False, m=m, esn0_db=ESN0_DB, seed=seed
    )
    _y, probes = demod(
        x,
        real=False,
        sps=sps,
        m_out=4,
        m=m,
        bn_timing=BN,
        bn_carrier=BN,
        freq_offset=offset,
    )
    return carrier_acquired(probes, offset, bound, sps)


def run_timing(m, sps, multiple, seed):
    """One timing trial: a clock error of `multiple` x `bn_timing`."""
    err = clock_offset_inside_bw(BN, multiple)
    x, _idx = make_signal(
        sps, NSYM, real=False, m=m, esn0_db=ESN0_DB, seed=seed
    )
    _y, probes = demod(
        x,
        real=False,
        sps=sps,
        m_out=4,
        m=m,
        bn_timing=BN,
        bn_carrier=BN,
        clock_offset=err,
    )
    return timing_acquired(probes, sps, err)


def sweep_carrier(orders=ORDERS, sps_choices=SPS_CHOICES, seeds=SEEDS):
    """Success fraction per (order, sps, multiple-of-the-bound)."""
    out = {}
    for m in orders:
        for sps in sps_choices:
            for mult in CARRIER_MULTIPLES:
                ok = sum(
                    run_carrier(m, sps, mult, 700 + s) for s in range(seeds)
                )
                out[(m, sps, mult)] = ok / seeds
    return out


def sweep_timing(sps_choices=SPS_CHOICES, seeds=SEEDS):
    """Success fraction per (sps, multiple-of-`bn_timing`), QPSK."""
    out = {}
    for sps in sps_choices:
        for mult in TIMING_MULTIPLES:
            ok = sum(run_timing(4, sps, mult, 900 + s) for s in range(seeds))
            out[(sps, mult)] = ok / seeds
    return out


def shoulders(frac_by_multiple):
    """`(reliable_to, collapses_at)` from a multiple -> fraction mapping.

    `reliable_to` is the largest multiple where every seed acquired;
    `collapses_at` the smallest where none did. Either may be None, and that
    is a result rather than a gap: no collapse inside the swept range means
    the range was too narrow to find one, which is worth reporting as such
    instead of quoting the last point as though it were a boundary.
    """
    mults = sorted(frac_by_multiple)
    reliable = [x for x in mults if frac_by_multiple[x] >= 1.0]
    dead = [x for x in mults if frac_by_multiple[x] <= 0.0]
    return (max(reliable) if reliable else None, min(dead) if dead else None)


def _report(title, rows, header):
    print(f"\n{title}")
    print("  " + header)
    for line in rows:
        print("  " + line)


def main():
    """Run both sweeps and print the envelope each one establishes."""
    print(
        f"pull-in characterization — bn {BN}, Es/N0 {ESN0_DB:g} dB, "
        f"{NSYM} symbols, {SEEDS} seeds/point, settle "
        f"{settle_floor(BN, BN)} symbols"
    )

    car = sweep_carrier()
    rows = []
    for m in ORDERS:
        for sps in SPS_CHOICES:
            byx = {x: car[(m, sps, x)] for x in CARRIER_MULTIPLES}
            rel, dead = shoulders(byx)
            cells = " ".join(f"{byx[x]:>4.2f}" for x in CARRIER_MULTIPLES)
            rows.append(
                f"M={m} sps={sps:<3} {cells}   reliable<={rel}  dead>={dead}"
            )
    _report(
        "CARRIER — success fraction vs offset in multiples of bn_carrier/m",
        rows,
        "            "
        + " ".join(f"{x:>4g}" for x in CARRIER_MULTIPLES)
        + "   shoulders",
    )

    tim = sweep_timing()
    rows = []
    for sps in SPS_CHOICES:
        byx = {x: tim[(sps, x)] for x in TIMING_MULTIPLES}
        rel, dead = shoulders(byx)
        cells = " ".join(f"{byx[x]:>4.2f}" for x in TIMING_MULTIPLES)
        rows.append(
            f"sps={sps:<3}      {cells}   reliable<={rel}  dead>={dead}"
        )
    _report(
        "TIMING — success fraction vs clock error in multiples of bn_timing",
        rows,
        "            "
        + " ".join(f"{x:>4g}" for x in TIMING_MULTIPLES)
        + "   shoulders",
    )

    print(
        "\nThe carrier row is the one to read across M: the multiple at which "
        "it collapses should NOT move with the order, because the bound "
        "already carries the 1/m. If it does, the units are wrong — which is "
        "the defect doppler#843 was."
    )


if __name__ == "__main__":
    main()
