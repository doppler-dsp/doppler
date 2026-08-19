"""`MpskReceiver` vs `MpskReceiverR` across oversampling: EVM and lock time.

The two receivers exist for different inputs, not different quality, so the
useful test is the pair on ONE stimulus at several oversampling ratios. The
real path is the constrained one and its design case is heavy oversampling of
an IF at fs/4 -- 40 MSa/s sampling a 10 MHz IF at Rs = 3 kS/s is `fc = 0.25`,
`sps = 13333` -- while the complex path is deliberately more flexible. Both are
measured here because "R is worse" and "both are limited by something shared"
look identical until you measure the two together.

What this module pins:

* neither path degrades as oversampling rises (the real case is the high end,
  and it is the one nobody had numbers for);
* the two paths CONVERGE at high oversampling, which locates the noiseless
  floor in the shared cascade rather than in the R2C halfband;
* both land on the coherent bound under AWGN at every ratio;
* lock time stays bounded, measured from the receivers' own verify-counted
  detectors.

Every measurement window starts at `settle_from(...)`, i.e. after the larger of
the analytic settling budget and both reported locks. See `_mpsk_rx_harness`
for why that budget is `2 * (5/bn_timing + 5/bn_carrier)` and not `5/Bn`.

**Every case presents an offset INSIDE the loop bandwidth** -- half of `Bn` on
each loop. That is what makes a lock-time assertion meaningful, and it is not a
conservative choice, it is the only defensible one. Measured carrier lock time
against offset, at `sps = 64`, `bn = 0.01`, budget 2000 symbols:

    df / Bn     R lock    C lock
      0.25          39       157
      0.50         237       247      <- what these tests use
      1.00        1376      1701
      2.00       never     never
      4.00       never     never

Inside `Bn` there is an order of magnitude of margin; AT `Bn` lock still lands
inside the budget but takes six times longer; beyond `2 * Bn` neither path
acquires at all -- and that is correct behaviour, not a defect, because pull-in
range is set by loop bandwidth. An assertion out there would pass or fail on
where the transient happened to push the integrator, which is why this table is
documentation and not a test. Widening pull-in is the loop bandwidth's job (a
higher-rate tap sees a proportionally wider range) or a coarse frequency
estimate's, passed in as `init_norm_freq`.

Note also that R and C fall off at the same offset, which is itself a result:
pull-in range here is a property of the symbol-rate NDA discriminator both
share, not of either front end.
"""

from __future__ import annotations

import numpy as np
import pytest

from ._mpsk_rx_harness import (
    DEFAULT_M,
    clock_offset_inside_bw,
    demod,
    freq_offset_inside_bw,
    lock_symbol,
    make_signal,
    settle_floor,
    settle_from,
    symbol_metrics,
)

# (label, sps, m_out, bn, nsym)
#
# `sps` must clear `sps > 2 * m_out` for the real path, so the low end is 20 at
# m_out = 8.
#
# **Every ratio uses the SAME loop bandwidth.** Loop bandwidth is normalised to
# the symbol rate, so the settling budget is a fixed number of SYMBOLS at every
# oversampling ratio -- which means a high-`sps` case needs proportionally more
# SAMPLES for the same measurement, and at `sps = 2048` that is ~4.9 M of them.
# Widening `bn` to shorten the record is not an economy, it is a different
# receiver: it changes the loop under test, its settling, its jitter and its
# pull-in range all at once, so the numbers would no longer describe the
# configuration anyone runs. Long records are simply the cost of characterising
# a heavily oversampled front end (and `wfmgen` exists for the ones too long to
# build in a test).
RATIOS = [
    ("low", 20, 8, 0.01, 2400),
    ("med", 64, 8, 0.01, 2400),
    ("high", 512, 8, 0.01, 2400),
    ("very-high", 2048, 8, 0.01, 2400),
]

PATHS = [("R", True), ("C", False)]

#: The AWGN cases settle later than the noiseless ones and need the room: the
#: complex path settles at ~3900 symbols against the real path's 2000.
_AWGN_NSYM = 12000


def _measure(real, sps, m_out, bn, nsym, esn0_db=None):
    """One case, with BOTH loops given something to acquire.

    Half of each loop's acquisition bound: a carrier offset of
    `bn/(2*m) * Rs` -- the `m` because the discriminator is an M-th power --
    and a sample-clock error of `bn/2`. Seeding the receiver exactly on truth
    would leave both loops idle in their initial state, and every lock time
    and EVM measured that way describes a receiver that never had to work.
    """
    x, idx = make_signal(sps, nsym, real=real, esn0_db=esn0_db)
    y, pr = demod(
        x,
        real=real,
        sps=sps,
        m_out=m_out,
        bn_timing=bn,
        bn_carrier=bn,
        freq_offset=freq_offset_inside_bw(bn, DEFAULT_M),
        clock_offset=clock_offset_inside_bw(bn),
    )
    settle = settle_from(pr, floor=settle_floor(bn, bn))
    if settle is None or len(y) - settle < 200:
        return None
    r = symbol_metrics(y, idx, settle=settle)
    return {
        "evm": r.evm_db,
        "m2m4": r.m2m4_db,
        "ser": r.ser,
        "lag": r.lag,
        "settle": settle,
        "t_lock": lock_symbol(pr["sync.locked"]),
        "c_lock": lock_symbol(pr["car.locked"]),
        "mu": pr["sync.mu"],
        "nsym_out": len(y),
    }


@pytest.mark.parametrize(
    "label,sps,m_out,bn,nsym", RATIOS, ids=[r[0] for r in RATIOS]
)
@pytest.mark.parametrize("path,real", PATHS, ids=[p[0] for p in PATHS])
def test_noiseless_decode_is_error_free(
    path, real, label, sps, m_out, bn, nsym
):
    """Every ratio, both paths: zero symbol errors and a real EVM floor.

    Noiseless input leaves no statistical excuse for an error, so SER is
    asserted exactly zero. The EVM bound is loose (-18 dB against -24 measured)
    because this test's job is to catch a path that stops working at some
    oversampling ratio, not to police the last dB -- that is
    `test_paths_converge_at_high_oversampling`'s job.
    """
    r = _measure(real, sps, m_out, bn, nsym)
    assert r is not None, (
        f"{path} at sps={sps}: no settled window — loops did not both lock "
        f"within {nsym} symbols (budget {settle_floor(bn, bn)})"
    )
    assert r["ser"] == 0.0, (
        f"{path} sps={sps}: noiseless SER {r['ser']} at lag {r['lag']} "
        f"(EVM {r['evm']:.1f} dB, window from {r['settle']})"
    )
    assert r["evm"] < -18.0, f"{path} sps={sps}: EVM {r['evm']:.1f} dB"
    # A saturated or undetected alignment used to be checked here, by hand,
    # against the lag the harness returned. `symbol_metrics` now refuses to
    # return an SER at all in that case, so reaching this line means an
    # alignment was detected.


@pytest.mark.parametrize(
    "label,sps,m_out,bn,nsym", RATIOS, ids=[r[0] for r in RATIOS]
)
@pytest.mark.parametrize("path,real", PATHS, ids=[p[0] for p in PATHS])
def test_evm_lands_on_the_coherent_bound(
    path, real, label, sps, m_out, bn, nsym, request
):
    """`EVM_dB = -(Es/N0)_dB` at every ratio, on both paths.

    The lower bound is the important half. An EVM that beats the coherent bound
    is not a good receiver, it is a broken measurement -- a mis-scaled noise
    variance, or a window that excluded the noise. Since the real and complex
    stimuli use DIFFERENT Es/N0 conventions (a real passband signal carries
    half
    its power at the negative frequency), this assertion is also what keeps
    those two conventions honest against each other.
    """
    esn0 = 12.0
    # sps=512 and sps=2048 were strict-xfail here on BOTH paths until the
    # front-end AGC landed, and the marker called the fix exactly: the level
    # was being set where the signal's share of a unit-power composite still
    # depended on the input oversampling (A^2 drive 0.76 at sps=20, 0.50 at
    # 64, 0.11 at 512, 0.03 at 2048), and a TED's slope goes as A^2. The AGC
    # now sits inside the cascade AFTER the decimation, where the noise has
    # been filtered to the terminal rate, so the level no longer moves with
    # sps -- and the carrier discriminator normalises by its own |z|^M instead
    # of leaning on an AGC downstream of the timing strobe. Every ratio runs
    # unmarked on both paths; strict=True is what turned this red the moment
    # it started passing, which is what it was for.
    r = _measure(real, sps, m_out, bn, max(nsym, _AWGN_NSYM), esn0_db=esn0)
    assert r is not None, f"{path} at sps={sps}: no settled window under AWGN"
    assert -esn0 - 1.0 < r["evm"] < -esn0 + 3.5, (
        f"{path} sps={sps}: EVM {r['evm']:.1f} dB against a {-esn0:.0f} dB "
        f"bound (SER {r['ser']:.4f}, window from {r['settle']})"
    )


@pytest.mark.parametrize(
    "label,sps,m_out,bn,nsym", RATIOS, ids=[r[0] for r in RATIOS]
)
@pytest.mark.parametrize("path,real", PATHS, ids=[p[0] for p in PATHS])
def test_both_loops_lock_within_their_budget(
    path, real, label, sps, m_out, bn, nsym
):
    """Lock time, from the receivers' own detectors, inside the joint budget.

    **The offset being INSIDE the loop bandwidth is what makes this a test.**
    Given an error a loop is specified to track -- half of `Bn` here, on both
    loops -- failing to declare within `2 * (5/bn_timing + 5/bn_carrier)` is a
    defect, full stop. Outside `Bn` the same failure is expected behaviour, so
    an assertion there would be measuring luck: acquisition depends on where
    the
    transient happens to push the integrator, and both outcomes are silent
    about
    correctness. Pull-in beyond `Bn` belongs in a characterisation sweep with a
    reported success fraction (and is what a wider loop and a coarse frequency
    estimate exist to address), never here.

    Timing lock is quantised to the detector's `avgs` (133 looks per decision),
    so it lands on 132, 265, ... -- always a bound, never an equality.
    """
    x, _ = make_signal(sps, nsym, real=real)
    _, pr = demod(
        x,
        real=real,
        sps=sps,
        m_out=m_out,
        bn_timing=bn,
        bn_carrier=bn,
        freq_offset=freq_offset_inside_bw(bn, DEFAULT_M),
        clock_offset=clock_offset_inside_bw(bn),
    )
    budget = settle_floor(bn, bn)
    t_lock = lock_symbol(pr["sync.locked"])
    c_lock = lock_symbol(pr["car.locked"])
    assert t_lock is not None, f"{path} sps={sps}: timing never locked"
    assert c_lock is not None, f"{path} sps={sps}: carrier never locked"
    assert t_lock <= budget, (
        f"{path} sps={sps}: timing locked at {t_lock}, budget {budget}"
    )
    assert c_lock <= budget, (
        f"{path} sps={sps}: carrier locked at {c_lock}, budget {budget}"
    )


def test_paths_converge_at_high_oversampling():
    """At heavy oversampling the two paths measure the SAME EVM, which is where
    the noiseless floor lives.

    This is the load-bearing structural claim. The complex path reaches ~-100
    dB
    only in the narrow set of geometries whose cascade needs no integer
    decimation stage; at any realistic oversampling it needs one, and then both
    paths sit at the same ~-24 dB. That places the floor in the shared CIC /
    halfband chain aliasing a rectangular pulse's sinc sidelobes -- NOT in the
    R2C halfband, which is what the real path alone has.

    If this test ever fails with R much worse than C, the shared-floor
    explanation is dead and the R2C is back under suspicion. That is exactly
    the
    diagnostic this file exists to provide, so the tolerance is tight (3 dB
    against a measured 0.1-0.2 dB spread).
    """
    sps, m_out, bn, nsym = 512, 8, 0.01, 2400
    r = _measure(True, sps, m_out, bn, nsym)
    c = _measure(False, sps, m_out, bn, nsym)
    assert r is not None and c is not None
    assert abs(r["evm"] - c["evm"]) < 3.0, (
        f"real {r['evm']:.1f} dB vs complex {c['evm']:.1f} dB at sps={sps}: "
        f"the paths no longer share a floor, so the limit is not the shared "
        f"cascade any more"
    )


def test_high_oversampling_is_not_worse_than_low():
    """Heavy oversampling is the real path's design case, so it must not be its
    worst case.

    Worth pinning because the intuition runs the other way: more samples per
    symbol means a longer rectangular pulse, a narrower sinc, and LESS energy
    in
    the decimator's stopband to fold back -- so the floor should improve or
    hold,
    never degrade. A regression that broke the cascade's stage planning at high
    decimation would show up here and nowhere else.
    """
    low = _measure(True, 20, 8, 0.01, 2400)
    high = _measure(True, 512, 8, 0.01, 2400)
    assert low is not None and high is not None
    assert high["evm"] < low["evm"] + 4.0, (
        f"real path degrades with oversampling: {low['evm']:.1f} dB at sps=20 "
        f"vs {high['evm']:.1f} dB at sps=512"
    )


def test_timing_nco_does_not_slip_at_any_ratio():
    """`sync.mu` -- the timing NCO's phase -- must park, not walk.

    Checked across ratios because a rate-only defect scales with `sps`: the
    tracked rate can read correct while the sampling phase slides, and only the
    accumulator's own phase shows it. One output period of slip is one symbol
    lost eventually.
    """
    for label, sps, m_out, bn, nsym in RATIOS:
        x, _ = make_signal(sps, nsym, real=True)
        _, pr = demod(
            x,
            real=True,
            sps=sps,
            m_out=m_out,
            bn_timing=bn,
            bn_carrier=bn,
            freq_offset=freq_offset_inside_bw(bn, DEFAULT_M),
            clock_offset=clock_offset_inside_bw(bn),
        )
        mu = pr["sync.mu"]
        assert np.all((mu >= 0.0) & (mu < 1.0)), f"{label}: mu out of [0, 1)"
        start = min(settle_floor(bn, bn), mu.size - 200)
        cum = np.unwrap(2.0 * np.pi * mu) / (2.0 * np.pi)
        slip = abs(cum[-1] - cum[start])
        assert slip < 3.0, (
            f"{label} sps={sps}: timing NCO slipped {slip:.2f} output periods"
        )
