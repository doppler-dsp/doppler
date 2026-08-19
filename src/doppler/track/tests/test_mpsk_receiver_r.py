"""Functional tests for `MpskReceiverR`, the real-IF M-PSK receiver.

Until this file was written the type had NO functional coverage: the scaffold
checked that the constructor returned non-NULL and that `reset` existed, and
nothing ever put a signal through it. Two real defects lived behind that gap
for as long as it did -- an undocumented placement tolerance (see
`test_occupied_band_may_touch_dc_but_not_overrun_it`) and an EVM floor nobody
had a number for -- so the tests here are deliberately about what the receiver
DELIVERS, not about whether its accessors round-trip.

Everything is measured at the design centre, `fc = fs/4`, because that is not
one point on a band -- it is what the architecture is FOR. An R2C halfband is
the cheapest real-to-complex converter there is, it bakes in the fs/4 shift for
free, and it decimates by two in the same pass; all three are the same fact.
It is also the realistic case: 40 MSa/s sampling a 10 MHz IF is exactly fs/4.

Off-centre placement is a TOLERANCE, not an advertised band, and it is
geometric: the occupied band `fc +- 1/sps` must not overrun DC or Nyquist. See
`docs/design/mpsk.md` section 1.3 for the derivation and the measured table.

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
    # Deliberately POSITIONAL: this is the one test that pins the constructor's
    # argument ORDER, which keyword calls everywhere else cannot catch. The
    # order is m, sps, m_out, pulse, rrc_beta, rrc_span, bn_carrier, zeta,
    # bn_timing, lock_thresh, init_norm_freq, differential, num_phases.
    # Removing a parameter silently re-binds every later argument here, so if
    # this file is edited for a signature change, check it element by element
    # rather than deleting the one that went.
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
        0.5,
        0.0,
        0,
        1024,
    )
    assert obj is not None
    # Read back the tail of the list, so a shift by one is a failure rather
    # than a construction that quietly meant something else.
    assert obj.m == 4 and obj.sps == 16.0 and obj.m_out == 4
    assert obj.lock_thresh == pytest.approx(0.5)
    assert obj.num_phases == 1024


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
    is no statistical excuse for a single error. The alignment behind that SER
    is `symbol_metrics`' detected one: it refuses to return a number at all
    when nothing detects, which is what a hand-written `abs(lag) < 190` used
    to (partially) check here.
    """
    x, idx = make_signal(32, 2400, real=True)
    y, pr = demod(x, real=True, sps=32, m_out=8)

    assert len(y) > 2300, f"only {len(y)} symbols from {len(x)} samples"
    settle = settle_from(pr)
    assert settle is not None, "both loops must lock before symbols are judged"
    r = symbol_metrics(y, idx, settle=settle)
    assert r.ser == 0.0, (
        f"noiseless SER {r.ser} (lag {r.lag}, EVM {r.evm_db:.1f} dB, "
        f"M2M4 {r.m2m4_db:.1f} dB)"
    )


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
    r = symbol_metrics(y, idx, settle=settle_from(pr))
    assert r.evm_db < -20.0, (
        f"noiseless EVM {r.evm_db:.1f} dB — floor regressed"
    )


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
    r = symbol_metrics(y, idx, settle=settle_from(pr))
    assert -esn0 - 1.0 < r.evm_db < -esn0 + 3.0, (
        f"EVM {r.evm_db:.1f} dB against a {-esn0:.0f} dB bound at "
        f"Es/N0 = {esn0:.0f} dB (SER {r.ser:.4f}, M2M4 {r.m2m4_db:.1f} dB)"
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


def test_fs4_is_the_design_centre():
    """The supported placement is `fc = fs/4`, and it must be clean there.

    This is not one point on a band -- it is what the real path is FOR. An R2C
    halfband is the cheapest real-to-complex converter there is, it bakes in
    the fs/4 shift for free (the rotation sequence is 1, j, -1, -j: sign flips
    and rail swaps, no multiplies) and it decimates by two in the same pass.
    All three are the same fact, and they are all a statement about fs/4.

    Deterministic to a few tenths of a dB across seeds, so a single-seed
    assertion is sound here. It was not always: see
    `test_occupied_band_may_touch_dc_but_not_overrun_it` for why the seed
    spread used to matter and no longer does.
    """
    sps, m_out, nsym = 10, 4, 3000
    for seed in (2, 5, 9):
        x, idx = make_signal(sps, nsym, real=True, fc=IF_FS4, seed=seed)
        y, pr = demod(x, real=True, sps=sps, m_out=m_out, fc=IF_FS4)
        settle = settle_from(pr)
        assert settle is not None, f"seed {seed}: no lock at the design centre"
        r = symbol_metrics(y, idx, settle=settle)
        assert r.ser == 0.0, f"seed {seed}: SER {r.ser} at fs/4"
        assert r.evm_db < -18.0, f"seed {seed}: EVM {r.evm_db:.1f} dB at fs/4"


@pytest.mark.parametrize("sps", [10, 16, 20])
def test_occupied_band_may_touch_dc_but_not_overrun_it(sps):
    """The tolerance around fs/4 is geometric, and it is about OVERRUN.

    The halfband's image rejection collapses at the band edges (-6.5 dB at
    0.01, -13.7 at 0.02, past -60 dB across the middle, symmetric about fs/4).
    But what that costs a SIGNAL is not set by where its centre sits: the
    leaked image is the signal's own conjugate, at `[-fc-B, -fc+B]` for a
    wanted band `[fc-B, fc+B]`. Those overlap exactly when `-fc+B > fc-B`,
    i.e. when `B > fc`. So the tolerance is

        1/sps < fc < 0.5 - 1/sps

    with no fixed frequency in it. Touching DC is free; overrunning it costs.
    Parametrized over sps because that is the whole claim -- a rule stated as
    a fixed band cannot be checked at more than one geometry, and the fixed
    band this replaced (0.06..0.44) is over-conservative at high oversampling:
    at sps = 20 it forbids fc in [0.05, 0.11], which measures better than
    -18 dB with zero SER.

    Asserted on SER rather than EVM. EVM degrades gracefully and monotonically
    across this axis, so any EVM threshold is a judgement call about where
    "degraded" starts; the error rate is what actually breaks, and it breaks
    on the side of the limit the geometry predicts.
    """
    B = 1.0 / sps
    nsym, m_out, seed = 3000, 4, 3

    def measure(fc):
        x, idx = make_signal(sps, nsym, real=True, fc=fc, seed=seed)
        y, pr = demod(x, real=True, sps=sps, m_out=m_out, fc=fc)
        settle = settle_from(pr)
        if settle is None:
            return None, None
        r = symbol_metrics(y, idx, settle=settle)
        return float(r.evm_db), float(r.ser)

    # At the limit the band TOUCHES DC: still error-free, on both sides.
    for fc, side in ((B, "DC"), (0.5 - B, "Nyquist")):
        evm, ser = measure(fc)
        assert evm is not None, (
            f"sps={sps}: no lock with the band touching {side} (fc={fc:.3f})"
        )
        assert ser == 0.0, (
            f"sps={sps}: band touching {side} (fc={fc:.3f}) gave SER {ser}, "
            f"but touching is not overrunning -- the image does not overlap "
            f"the wanted band until B > fc"
        )

    # Overrun by 0.7 B and the image genuinely overlaps: the receiver goes.
    # 0.7 rather than 0.5 because the margin is real -- 0.4 B past the limit
    # is still error-free at every sps measured, and 0.6 B breaks at three of
    # the four but not at sps = 10. 0.7 B fails to lock at all of them, which
    # is the unambiguous point to gate on.
    for fc, side in ((0.3 * B, "DC"), (0.5 - 0.3 * B, "Nyquist")):
        evm, ser = measure(fc)
        assert evm is None or ser > 1e-3, (
            f"sps={sps}: overrunning {side} by 0.7/sps (fc={fc:.3f}) gave "
            f"SER {ser} and EVM {evm:.1f} dB -- the occupied band is well "
            f"past the limit and the folded image should be overlapping the "
            f"wanted signal. If this has gone quiet the halfband's edge "
            f"behaviour changed and the tolerance needs re-deriving."
        )

    # And the design centre beats both limits, which is why it is the centre.
    centre_evm, centre_ser = measure(IF_FS4)
    limit_evm, _ = measure(B)
    assert centre_ser == 0.0
    assert centre_evm < limit_evm, (
        f"sps={sps}: fs/4 ({centre_evm:.1f} dB) should beat the band edge "
        f"({limit_evm:.1f} dB)"
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
