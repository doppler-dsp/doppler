"""Tests for the C-backed doppler.track.RateSync class."""

import numpy as np
import pytest

from doppler.ber import ber_evm_db, ber_settle_syms
from doppler.resample import MatchedRateConverter
from doppler.track import RateSync
from doppler.wfm import rrc_h

BETA = 0.35
SPAN = 8
NSYM = 3000


def _tx(
    sps: float, tau: float, nsym: int = NSYM, seed: int = 7, amp: float = 1.0
):
    """RRC-shaped BPSK at `sps` samples/symbol, timing offset `tau` symbols.

    ``amp`` is the CONTRACTED unit symbol amplitude, matching the C twin
    (`native/tests/test_ratesync_core.c` via `dp_tx_test.h`). It used to
    default to 0.25 "well inside the CIC's +-1.0 input bound", and both
    halves of that were wrong: the bound is 2.0 (``CIC_PAPR_HEADROOM``
    reserves exactly the 6 dB an RRC's 1.582 peak needs, so unit amplitude
    fits), and a quarter-amplitude stream drives a Gardner TED at ``A^2``
    -- a sixteenth of the loop gain ``bn`` names.

    That is not a small mismeasurement, because it makes the SETTLING
    BUDGET wrong too: `ber_settle_syms(bn)` assumes the loop runs at `bn`,
    so at 0.25 the real budget is 16x longer than the record these tests
    use. The old "final quarter" EVM window hid it by starting at 75% of
    the record; the canonical window does not, which is how this surfaced.
    """
    syms = np.where(
        np.random.default_rng(seed).integers(0, 2, nsym) > 0, 1.0, -1.0
    )
    n = int(nsym * sps) + 64
    idx = np.arange(n, dtype=np.float64)
    x = np.zeros(n)
    for k, a in enumerate(syms):
        t = (idx - (k + SPAN) * sps) / sps - tau
        near = np.abs(t) <= SPAN
        x[near] += a * rrc_h(t[near], BETA)
    return (amp * x).astype(np.complex64), syms


def _evm_db(y: np.ndarray, bn: float = 0.01) -> float:
    """Steady-state EVM (dB), from the library's own primitives.

    `ber_evm_db` is the canonical self-referenced EVM and `ber_settle_syms`
    is the canonical answer to where a settled window may START. Both were
    hand-written here: a least-squares EVM over "the final quarter". A
    window pinned to a FRACTION of the record is the documented way a
    receiver test measures the acquisition transient and reports it as
    steady state -- and the fraction stops meaning the same thing the
    moment a caller passes a different `nsym`, which these tests do.

    A window containing an acquisition cycle slip reads ~20 dB worse with a
    perfectly open eye, so let ``lock_stat`` make the lock decision.
    Returns 0.0 dB (the primitive's "no lock" answer) when the record is
    too short to contain a settled window.
    """
    y = np.asarray(y)
    lo = int(ber_settle_syms(bn, 0.0))
    if y.size < lo + 100:
        return 0.0
    return float(ber_evm_db(y, lo, y.size, 2))


# ------------------------------------------------------------------ #
# Construction                                                        #
# ------------------------------------------------------------------ #


def test_create_defaults():
    rs = RateSync()
    assert rs.rate == pytest.approx(4.0)
    assert rs.locked is False
    assert rs.clipped is False
    assert rs.ctrl == 0.0


@pytest.mark.parametrize(
    "kw",
    [
        {"beta": 1.5},
        {"beta": -0.1},
        {"span": 0},
        {"m": 1},  # no half-symbol gate exists
        {"m": 3},  # odd: the gate would not land on m/2
        {"m": 16},  # beyond the in-struct strobe ring
        {"num_phases": 1000},  # not a power of two
        {"zeta": 0.0},
        {"bn": -0.01},
        {"sps": 1.5},  # sps < m would make the terminal stage interpolate
    ],
)
def test_invalid_params_raise(kw):
    with pytest.raises(ValueError):
        RateSync(**{"sps": 4.0, **kw})


@pytest.mark.parametrize("pulse", ["sinc", "RRC", ""])
def test_unknown_pulse_raises(pulse):
    with pytest.raises(ValueError):
        RateSync(sps=4.0, pulse=pulse)


# ------------------------------------------------------------------ #
# The object owns a matched cascade -- it builds no filters itself    #
# ------------------------------------------------------------------ #


@pytest.mark.parametrize("sps", [4.0, 17.333333333, 64.0, 256.0])
def test_bank_is_constant_in_input_rate(sps):
    # RateSync asks for rate = m/sps and the planner puts the integer part in
    # HB/CIC stages, so the matched filter is sized by the POST-decimation
    # rate. A 64x span of input rates leaves the bank the same size -- that is
    # what makes a matched filter affordable at 256 samples per symbol.
    rc = MatchedRateConverter(
        rate=2 / sps, compensate=1, pulse="rrc", pulse_sps=2.0
    )
    arms, taps = rc.bank_shape
    assert arms == 1024
    assert taps < 4 * SPAN * 2 + 16
    assert rc.stages[-1].endswith(",rrc)")


# ------------------------------------------------------------------ #
# Lock and tracking                                                   #
# ------------------------------------------------------------------ #


@pytest.mark.parametrize(
    "sps",
    [
        4.0,  # HB + Resampler(1.0)
        17.333333333,  # CIC(8) + Resampler(0.923)  -- the arbitrary-rate case
        64.0,  # CIC(32) + Resampler(1.0)
    ],
)
def test_locks_from_every_initial_offset(sps):
    # Three cascades whose planned shapes differ in the ways that matter: a
    # halfband front end, a CIC with a fractional terminal rate, and a CIC
    # with a terminal rate of exactly 1.0 (where one input can complete two
    # output periods). Judge lock by lock_stat, and EVM only where the loop
    # has settled -- see _evm_db.
    for tau in (0.0, 0.25, 0.5, 0.75):
        x, _ = _tx(sps, tau, nsym=6000)
        rs = RateSync(sps=sps, pulse="rrc", beta=BETA, span=SPAN, m=2, bn=0.01)
        y = rs.steps(x)
        assert rs.lock_stat > 0.55, f"tau={tau} lock_stat={rs.lock_stat}"
        assert rs.locked is True
        assert _evm_db(y) < -32.0, f"tau={tau} evm={_evm_db(y)}"
        assert rs.clipped is False


def test_worst_case_acquisition_is_slow_but_converges():
    """The T/2 equilibrium is the worst start, and the loop still settles
    inside the library's own budget.

    Starting exactly ON the unstable half-symbol equilibrium is the worst
    case: the detector has almost no error there, so the loop is repelled
    only slowly. The point of the test is that a regression in acquisition
    TIME cannot hide behind a steady-state EVM measured at the very end.

    What it pins is `ber_settle_syms(bn)` -- the library's own answer for
    where steady state may start -- rather than a hand-counted symbol
    number. Measured at unit amplitude (bn = 0.01, budget 1000 symbols):
    quarters read -17.8 / -36.5 / -36.0 / -36.1 dB, and a 500-symbol window
    is already below -30 dB by ~400 symbols, so the budget is conservative
    and the loop beats it by 2.5x.

    This test used to pin "~2500 symbols", which was an artifact of a
    quarter-amplitude stimulus: a Gardner TED's slope goes as A^2, so the
    loop ran at a sixteenth of the bandwidth `bn` named and genuinely was
    that slow. Driving it at the contracted amplitude made acquisition ~4x
    faster and the old bound unsatisfiable in the correct direction --
    the first quarter is no longer bad enough to read "still acquiring"
    against -15 dB.
    """
    bn = 0.01
    x, _ = _tx(4.0, 0.0, nsym=8000)
    rs = RateSync(sps=4.0, pulse="rrc", m=2, bn=bn)
    y = np.asarray(rs.steps(x))
    q = len(y) // 4
    quarters = [ber_evm_db(y, i * q, (i + 1) * q, 2) for i in range(4)]

    # The transient is real and must still be visible, or this test would
    # pass on a loop that never had to acquire at all.
    assert quarters[0] > -25.0, f"no acquisition transient: {quarters}"
    # Settled by the library's budget, and STAYING settled.
    settle = int(ber_settle_syms(bn, 0.0))
    assert ber_evm_db(y, settle, len(y), 2) < -30.0, f"{quarters}"
    for i in (1, 2, 3):
        assert quarters[i] < -30.0, f"quarter {i} of {quarters}"


@pytest.mark.parametrize("actual", [8.0, 8.008, 7.992])
def test_tracks_an_unknown_clock_offset(actual):
    # The point of an arbitrary-rate receiver: transmit at a clock the
    # receiver was not told about (+-1000 ppm here) and let the loop find it.
    x, _ = _tx(actual, 0.2)
    rs = RateSync(sps=8.0, pulse="rrc", beta=BETA, span=SPAN, m=2, bn=0.005)
    rs.steps(x)
    assert rs.rate == pytest.approx(actual, abs=0.01)


def test_arbitrary_non_integer_sps():
    # sps need not be an integer, or a ratio of small integers.
    #
    # amp=1.0 is the CONTRACT, not a tuned value. RateSync is a timing block,
    # not a receiver: it carries no AGC, deliberately, because every receiver
    # composing it already levels in its own front-end cascade and a second
    # one inside the timing block would be two AGCs integrating against each
    # other. So the caller owns the level, and the level the TED's
    # construct-time slope was computed for is unit-amplitude symbols --
    # `ref_db = 10*log10(bank_e0 / bank_sps)` is ~0 dB precisely because the
    # bank normalises by its own pulse energy.
    #
    # Presenting anything else costs EVM with nothing to reveal it: measured
    # on this case, amp=0.25 gives -21.6 dB against -37.0 dB at unit
    # amplitude -- 15 dB gone with lock_stat 0.70 either way and
    # clipped=False, because clipping is the OVER-drive failure and there is
    # no under-drive twin. Tracked as gh-661.
    x, _ = _tx(17.333333333, 0.37, amp=1.0)
    rs = RateSync(sps=17.333333333, pulse="rrc", bn=0.01)
    y = rs.steps(x)
    assert rs.lock_stat > 0.55
    assert not rs.clipped, "unit amplitude must not overdrive the CIC"
    assert _evm_db(y) < -32.0
    assert len(y) == pytest.approx(NSYM, rel=0.02)


def test_iandd_pulse_locks_on_rectangular_symbols():
    # The rectangle is one symbol wide whatever span says, so its matched
    # filter is much cheaper -- and it is the right one for an NRZ link.
    sps = 8.0
    syms = np.where(
        np.random.default_rng(3).integers(0, 2, NSYM) > 0, 1.0, -1.0
    )
    x = (0.25 * np.repeat(syms, int(sps))).astype(np.complex64)
    # It needs m >= 4 though: at m=2 the filter is a two-tap sum, too coarse
    # for the eye to open (see the `m` docs).
    rs = RateSync(sps=sps, pulse="iandd", m=4, bn=0.01)
    y = rs.steps(x)
    assert rs.lock_stat > 0.55
    assert rs.rate == pytest.approx(sps, abs=0.02)
    assert len(y) == pytest.approx(NSYM, rel=0.02)


def test_iandd_at_m2_is_documented_as_too_coarse():
    # Pin the stated reason for the m >= 4 guidance so the doc cannot quietly
    # drift from the behaviour.
    #
    # The reason is an EVM argument, and mpsk_receiver_create()'s own m_out
    # documentation makes it in those terms: the rectangle is one symbol
    # wide, so its matched filter is an m_out-tap sum spanning it, and a
    # smaller m_out samples the same integral more coarsely.
    #
    # This used to be asserted on `lock_stat < 0.3`, and that proxy is gone:
    # since the TED normalises by its own construct-time slope rather than
    # by a running power average, the timing loop locks perfectly well at
    # m_out = 2 (measured lock_stat 0.94, ABOVE m_out = 4's 0.88) while
    # still costing ~8 dB of EVM against m_out = 8. That is not a broken
    # lock detector -- the loop really does lock -- it is the reminder that
    # locking and demodulating well are different claims, and only the
    # second one is what the m >= 4 guidance is about.
    sps = 8.0
    syms = np.where(
        np.random.default_rng(3).integers(0, 2, NSYM) > 0, 1.0, -1.0
    )
    x = (0.25 * np.repeat(syms, int(sps))).astype(np.complex64)

    evm = {}
    for m_out in (2, 4, 8):
        rs = RateSync(sps=sps, pulse="iandd", m=m_out, bn=0.01)
        evm[m_out] = _evm_db(rs.steps(x))

    # Coarser is monotonically worse (EVM in dB, so larger is worse) ...
    assert evm[2] > evm[4] > evm[8], f"not monotone in m_out: {evm}"
    # ... and m_out = 2 is worse by enough to justify the guidance.
    assert evm[2] - evm[8] > 5.0, (
        f"m_out=2 costs only {evm[2] - evm[8]:.1f} dB against m_out=8; "
        f"the m >= 4 guidance no longer has its stated basis: {evm}"
    )


# ------------------------------------------------------------------ #
# Streaming contract                                                  #
# ------------------------------------------------------------------ #


def test_block_boundary_invariant():
    # State carries across calls, so chunking must not change the symbols.
    x, _ = _tx(17.333333333, 0.3)
    kw = {"sps": 17.333333333, "pulse": "rrc", "m": 2, "bn": 0.01}
    whole = np.array(RateSync(**kw).steps(x))
    rs = RateSync(**kw)
    cut = len(x) // 3
    chunked = np.concatenate(
        [np.array(rs.steps(x[:cut])), np.array(rs.steps(x[cut:]))]
    )
    assert np.array_equal(whole, chunked)


def test_reset_restores_post_create_behaviour():
    x, _ = _tx(4.0, 0.4)
    rs = RateSync(sps=4.0, pulse="rrc", m=2, bn=0.01)
    first = np.array(rs.steps(x))
    rs.reset()
    assert rs.ctrl == 0.0
    assert rs.locked is False
    assert np.array_equal(first, np.array(rs.steps(x)))


def test_clipped_reports_an_overdriven_front_end():
    # An overdriven CIC costs ~25 dB of EVM with a perfectly healthy lock, so
    # no timing metric reveals it. This is the only signal.
    x, _ = _tx(64.0, 0.1)
    rs = RateSync(sps=64.0, pulse="rrc", m=2, bn=0.01)
    rs.steps(x)
    assert rs.clipped is False
    rs.steps((x * 8).astype(np.complex64))
    assert rs.clipped is True


# ------------------------------------------------------------------ #
# Configuration                                                       #
# ------------------------------------------------------------------ #


def test_configure_preserves_the_lock():
    x, _ = _tx(4.0, 0.2)
    rs = RateSync(sps=4.0, pulse="rrc", m=2, bn=0.01)
    rs.steps(x)
    assert rs.locked is True
    rate_before = rs.rate
    rs.configure(0.002, 0.707)  # retune does not touch the integrator
    assert rs.bn == pytest.approx(0.002)
    assert rs.rate == pytest.approx(rate_before)
    assert rs.locked is True


def test_bn_property_round_trips():
    rs = RateSync(sps=4.0)
    rs.bn = 0.004
    assert rs.bn == pytest.approx(0.004)


def test_configure_lock_raw_drops_the_lock():
    x, _ = _tx(4.0, 0.2)
    rs = RateSync(sps=4.0, pulse="rrc", m=2, bn=0.01)
    rs.steps(x)
    assert rs.locked is True
    # Re-tuning must clear the in-flight block so the next decision uses only
    # looks gathered under the new geometry.
    rs.configure_lock_raw(64, 0.5, 0.4, 2, 4)
    assert rs.locked is False
    assert rs.lock_stat == 0.0


def test_context_manager_and_destroy():
    with RateSync(sps=4.0) as rs:
        assert rs.rate == pytest.approx(4.0)
    rs = RateSync(sps=4.0)
    rs.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        rs.steps(np.zeros(16, dtype=np.complex64))
