"""Functional tests for `MpskReceiverR`, the real-IF M-PSK receiver.

Until this file was written the type had NO functional coverage: the scaffold
checked that the constructor returned non-NULL and that `reset` existed, and
nothing ever put a signal through it. Two real defects lived behind that gap
for as long as it did -- an undocumented usable band (see
`test_usable_band_is_the_input_constraint`) and an EVM floor nobody had a
number for -- so the tests here are deliberately about what the receiver
DELIVERS, not about whether its accessors round-trip.

Everything is measured at the design centre, `fc = fs/4`, which is what the
architecture is built for: the R2C halfband bakes in a +fs/4 shift, so a real
IF belongs there. That is also the realistic case (40 MSa/s sampling a 10 MHz
IF is exactly fs/4).

The cross-path comparison against `MpskReceiver` across oversampling ratios
lives in `test_mpsk_receiver_oversampling.py`.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.track import MpskReceiverR

from ._mpsk_rx_harness import (
    IF_FS4,
    SETTLE_SYMS,
    demod,
    lock_symbol,
    make_signal,
    settle_from,
    symbol_metrics,
)

CTOR = {"m": 4, "sps": 32.0, "m_out": 8}


def test_create():
    obj = MpskReceiverR(
        4,
        16.0,
        4,
        "iandd",
        0.35,
        8,
        0.01,
        0.707,
        0.01,
        0,
        0.5,
        0.0,
        100,
        0,
        1024,
    )
    assert obj is not None


def test_context_manager():
    with MpskReceiverR(**CTOR) as rx:
        assert rx.sps == 32.0


def test_destroy():
    obj = MpskReceiverR(**CTOR)
    obj.destroy()


def test_defaults_construct_and_clear_the_sps_bound():
    """The default `sps` exists to clear `sps > 2 * m_out`.

    `MpskReceiverR` needs `sps > 2 * m_out` (the cascade behind the R2C
    halfband runs at twice the overall rate, and `Ddcr` needs that below
    0.5), so this type's two defaults are coupled in a way the complex
    twin's are not: raising `m_out` to 8 is what forces `sps` to 32.  A
    regression here does not degrade quietly -- the no-argument
    constructor stops working at all -- so pin both, and pin the bound
    that ties them together.
    """
    rx = MpskReceiverR()
    assert rx.m == 4 and rx.m_out == 8 and rx.sps == 32.0
    assert rx.sps > 2 * rx.m_out  # the constraint the default must clear

    # One notch below the bound is a ValueError, not a silent re-plan:
    # 2 * m_out is excluded (strictly greater), so 16.0 must fail at 8.
    with pytest.raises(ValueError):
        MpskReceiverR(m=4, sps=16.0, m_out=8)


# ── the functional core: does it actually demodulate? ────────────────────


def test_decodes_a_real_if_burst_at_the_design_centre():
    """The test this type never had: put a real IF through it and check the
    symbols come out.

    Noiseless, so the only errors possible are the receiver's own. SER is
    asserted to be EXACTLY zero over the settled window -- with no noise there
    is no statistical excuse for a single error -- and the lag is checked away
    from the search bounds, because a saturated lag search reports a plausible
    SER for the wrong alignment.
    """
    x, idx = make_signal(32, 2400, real=True)
    y, pr = demod(x, real=True, sps=32, m_out=8)

    assert len(y) > 2300, f"only {len(y)} symbols from {len(x)} samples"
    settle = settle_from(pr)
    assert settle is not None, "both loops must lock before symbols are judged"
    evm, ser, lag = symbol_metrics(y, idx, settle=settle)
    assert ser == 0.0, f"noiseless SER {ser} (lag {lag}, EVM {evm:.1f} dB)"
    assert lag is not None and abs(lag) < 190, f"lag search saturated at {lag}"


def test_noiseless_evm_floor():
    """Pin the noiseless floor, which is a real number and not zero.

    Measured -24 to -27 dB across every geometry tested. It is NOT a limit of
    the real path: the complex twin measures the same -24.5 dB whenever its
    cascade also contains an integer decimation stage, which at any realistic
    oversampling it does. Both are limited by the same CIC/halfband chain
    aliasing a rectangular pulse's sinc sidelobes (see
    `test_mpsk_receiver_oversampling.py::test_paths_agree_at_high_oversampling`).

    -20 dB leaves ~4 dB of margin on the measurement while still failing loudly
    if the floor moves by an order of magnitude -- which is what a broken arm,
    bank layout or tuning law would do.
    """
    x, idx = make_signal(32, 2400, real=True)
    y, pr = demod(x, real=True, sps=32, m_out=8)
    evm, _, _ = symbol_metrics(y, idx, settle=settle_from(pr))
    assert evm < -20.0, f"noiseless EVM {evm:.1f} dB — floor regressed"


def test_evm_lands_on_the_coherent_bound_under_awgn():
    """At `Es/N0 = X` the matched-filter bound is `EVM_dB = -X`.

    Checked from BOTH sides, and the lower one is the load-bearing half: an EVM
    that beats the bound is impossible, so it means the harness is wrong (a
    mis-scaled noise variance, or a window that excluded the noisy part). A
    one-sided assertion would have passed happily on exactly that bug.
    """
    esn0 = 12.0
    x, idx = make_signal(32, 2400, real=True, esn0_db=esn0)
    y, pr = demod(x, real=True, sps=32, m_out=8)
    evm, ser, _ = symbol_metrics(y, idx, settle=settle_from(pr))
    assert -esn0 - 1.0 < evm < -esn0 + 3.0, (
        f"EVM {evm:.1f} dB against a {-esn0:.0f} dB bound at "
        f"Es/N0 = {esn0:.0f} dB (SER {ser:.4f})"
    )


def test_both_loops_lock_and_report_it():
    """Lock is reported by verify-counted detectors, so use them.

    The timing detector averages 133 looks per decision, so its lock time is
    quantised to multiples of that (132, 265, ...) -- assert a bound, never an
    exact symbol. Both loops must also STILL be locked at the end of the burst:
    a detector that declares and drops has not locked.
    """
    x, _ = make_signal(32, 2400, real=True)
    _, pr = demod(x, real=True, sps=32, m_out=8)

    t_lock = lock_symbol(pr["sync.locked"])
    c_lock = lock_symbol(pr["car.locked"])
    assert t_lock is not None, "timing loop never declared lock"
    assert c_lock is not None, "carrier loop never declared lock"
    assert t_lock <= 4 * 133, f"timing lock took {t_lock} symbols"
    assert c_lock <= 1000, f"carrier lock took {c_lock} symbols"


def test_timing_nco_settles_rather_than_slipping():
    """`sync.mu` is the timing NCO's phase, so a settled loop parks it.

    Cumulative slip is the diagnostic: unwrapped `mu` drifting by whole output
    periods means a residual rate error the loop has not absorbed, which a
    tracked-rate check alone can miss (the rate can read correct while the
    phase walks). Bound it well above the measured noise -- observed under
    0.2 output periods over the burst -- so this catches a broken
    accumulator, not jitter.
    """
    x, _ = make_signal(32, 2400, real=True)
    _, pr = demod(x, real=True, sps=32, m_out=8)
    mu = pr["sync.mu"]
    assert mu.size > 2300
    assert np.all((mu >= 0.0) & (mu < 1.0)), "mu is a phase in [0, 1)"

    cum = np.unwrap(2.0 * np.pi * mu) / (2.0 * np.pi)
    slip = abs(cum[-1] - cum[SETTLE_SYMS])
    assert slip < 3.0, f"timing NCO slipped {slip:.2f} output periods"


def test_usable_band_is_the_input_constraint():
    """The R2C halfband's image rejection collapses at the band edges, and that
    -- not the symbol rate -- is what bounds where a real IF may sit.

    Measured on the front end alone: rejection is past -100 dB across roughly
    0.06..0.44 but only -7 dB at 0.01 and -14 dB at 0.02, and it is symmetric
    about fs/4. So the constraint is on the signal's OCCUPIED BAND, not its
    centre: a rectangular pulse spans `fc +- 1/sps` to its first null, and when
    that reaches an edge the folded image lands on the wanted signal.

    This is the defect that hid behind the missing tests, and it presents as a
    receiver bug at low oversampling: at `sps = 10` the pulse is +-0.1 wide, so
    an IF at 0.10 reaches DC and EVM collapses, while the SAME geometry at fs/4
    is clean. Pin both, so a future reader sees that the geometry is fine and
    the placement was not.

    **Averaged over seeds, because the penalty is bimodal.** The leaked image
    is the signal's own conjugate, so the damage depends on the symbol sequence
    and on which of the M rotations the carrier loop settles into. Measured
    over 12 usable seeds it lands in one of two states -- ~18 dB (9 seeds) or
    ~2.6 dB (3 seeds) -- with median 18.1 and mean 14.1. A single-seed
    assertion of "> 10 dB" therefore passes or fails on the draw; this asserted
    exactly that on seed 3 until 2026-07-27, and was green only because seed 3
    is one of the favourable nine. The median over several seeds is stable;
    individual seeds are not.
    """
    # 3000 symbols: the settling budget alone is 2000 (see settle_floor), and
    # this geometry's timing detector does not declare until symbol ~1063, so a
    # shorter burst leaves no settled window to judge.
    sps, m_out, nsym = 10, 4, 3000
    penalties, centres = [], []
    for seed in range(2, 11):
        ctr_x, ctr_idx = make_signal(
            sps, nsym, real=True, fc=IF_FS4, seed=seed
        )
        ctr_y, ctr_pr = demod(
            ctr_x, real=True, sps=sps, m_out=m_out, fc=IF_FS4
        )
        ctr_settle = settle_from(ctr_pr)
        if ctr_settle is None:
            # `make_signal` without `esn0_db` is perfectly noiseless, and
            # seeded exactly on centre the M-th-power loop can sit at its
            # measure-zero unstable equilibrium and never declare. That is the
            # documented reason the C twin adds light noise -- not a placement
            # effect -- so such a seed says nothing about the band edge.
            continue
        ctr_evm, ctr_ser, _ = symbol_metrics(ctr_y, ctr_idx, settle=ctr_settle)

        edge_x, edge_idx = make_signal(
            sps, nsym, real=True, fc=0.10, seed=seed
        )
        edge_y, _pr = demod(edge_x, real=True, sps=sps, m_out=m_out, fc=0.10)
        # Judged over the SAME window as its centre twin, so the comparison is
        # not confounded by two different measurement intervals.
        edge_evm, _, _ = symbol_metrics(edge_y, edge_idx, settle=ctr_settle)

        assert ctr_evm < -18.0, (
            f"seed {seed}: at fs/4 EVM should be good, got {ctr_evm:.1f} dB"
        )
        assert ctr_ser == 0.0, (
            f"seed {seed}: at fs/4 SER should be 0, {ctr_ser}"
        )
        centres.append(ctr_evm)
        penalties.append(edge_evm - ctr_evm)

    assert len(penalties) >= 6, (
        f"only {len(penalties)} of 9 seeds locked at the design centre; too "
        f"few to judge a median"
    )
    median = float(np.median(penalties))
    assert median > 10.0, (
        f"median edge penalty {median:.1f} dB over {len(penalties)} seeds "
        f"(individual: {[round(p, 1) for p in penalties]}); an occupied band "
        f"reaching DC should be far worse than the same geometry at fs/4. If "
        f"this has collapsed the halfband's edge behaviour changed and the "
        f"documented input constraint needs revisiting -- but check the "
        f"INDIVIDUAL values first, since roughly a quarter of seeds "
        f"legitimately land in the low state."
    )


def test_reset_returns_the_receiver_to_a_cold_start():
    """`reset` must clear the loops, not just the counters: the same input fed
    twice around a reset has to produce the same output, including the
    acquisition transient. Comparing only the settled tail would pass on a
    receiver whose reset left the integrator loaded.
    """
    x, _ = make_signal(32, 900, real=True)
    rx = MpskReceiverR(**CTOR, bn_timing=0.01, init_norm_freq=IF_FS4)
    first = rx.steps(x)
    rx.reset()
    second = rx.steps(x)
    assert len(first) == len(second)
    np.testing.assert_array_equal(first, second)
