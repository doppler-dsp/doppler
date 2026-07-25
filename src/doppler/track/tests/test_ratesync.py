"""Tests for the C-backed doppler.track.RateSync class."""

import math

import numpy as np
import pytest

from doppler.resample import MatchedRateConverter
from doppler.track import RateSync

BETA = 0.35
SPAN = 8
NSYM = 3000


def _rrc_h(t: np.ndarray, beta: float = BETA) -> np.ndarray:
    t = np.asarray(t, dtype=np.float64)
    out = np.empty_like(t)
    zero = np.abs(t) < 1e-9
    sing = (
        np.isclose(np.abs(t), 1 / (4 * beta), atol=1e-9)
        if beta > 0
        else np.zeros_like(t, dtype=bool)
    )
    gen = ~(zero | sing)
    out[zero] = 1 - beta + 4 * beta / math.pi
    if np.any(sing):
        a = math.pi / (4 * beta)
        out[sing] = (beta / math.sqrt(2)) * (
            (1 + 2 / math.pi) * math.sin(a) + (1 - 2 / math.pi) * math.cos(a)
        )
    tg = t[gen]
    pt = math.pi * tg
    num = np.sin(pt * (1 - beta)) + 4 * beta * tg * np.cos(pt * (1 + beta))
    den = pt * (1 - (4 * beta * tg) ** 2)
    out[gen] = num / den
    return out


def _tx(
    sps: float, tau: float, nsym: int = NSYM, seed: int = 7, amp: float = 0.25
):
    """RRC-shaped BPSK at `sps` samples/symbol, timing offset `tau` symbols.

    ``amp`` keeps the signal well inside the CIC's +-1.0 input bound. An
    overdriven front end costs ~25 dB of EVM with a perfectly healthy lock,
    which no timing metric reveals -- that is what ``clipped`` is for.
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
        x[near] += a * _rrc_h(t[near])
    return (amp * x).astype(np.complex64), syms


def _evm_db(y: np.ndarray) -> float:
    """Steady-state EVM against the LS-scaled hard decision, final quarter.

    A window containing an acquisition cycle slip reads ~20 dB worse with a
    perfectly open eye, so measure where the loop has settled and let
    ``lock_stat`` make the lock decision.
    """
    y = np.asarray(y)[3 * len(y) // 4 :]
    if len(y) < 100:
        return 0.0
    d = np.where(y.real >= 0, 1.0, -1.0)
    g = float(np.dot(d, y.real) / len(d))
    if g == 0.0:
        return 0.0
    return 20 * math.log10(
        float(np.linalg.norm(y - g * d) / (abs(g) * math.sqrt(len(d))))
    )


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
    # Starting exactly ON the unstable T/2 equilibrium is the worst case: the
    # detector has almost no error there, so the loop is repelled only slowly.
    # It does converge -- ~2500 symbols at bn=0.01 -- and this pins that
    # number so a regression in acquisition time cannot hide behind a
    # steady-state EVM measured at the very end.
    x, _ = _tx(4.0, 0.0, nsym=8000)
    rs = RateSync(sps=4.0, pulse="rrc", m=2, bn=0.01)
    y = np.asarray(rs.steps(x))
    q = len(y) // 4
    quarters = []
    for i in range(4):
        seg = y[i * q : (i + 1) * q]
        d = np.where(seg.real >= 0, 1.0, -1.0)
        g = float(np.dot(d, seg.real) / len(d))
        quarters.append(
            20
            * math.log10(
                float(
                    np.linalg.norm(seg - g * d) / (abs(g) * math.sqrt(len(d)))
                )
            )
        )
    assert quarters[0] > -15.0  # still acquiring
    assert quarters[2] < -30.0  # settled by the halfway point
    assert quarters[3] < -30.0


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
    x, _ = _tx(17.333333333, 0.37)
    rs = RateSync(sps=17.333333333, pulse="rrc", bn=0.01)
    y = rs.steps(x)
    assert rs.lock_stat > 0.55
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
    sps = 8.0
    syms = np.where(
        np.random.default_rng(3).integers(0, 2, NSYM) > 0, 1.0, -1.0
    )
    x = (0.25 * np.repeat(syms, int(sps))).astype(np.complex64)
    coarse = RateSync(sps=sps, pulse="iandd", m=2, bn=0.01)
    coarse.steps(x)
    fine = RateSync(sps=sps, pulse="iandd", m=4, bn=0.01)
    fine.steps(x)
    assert coarse.lock_stat < 0.3 < fine.lock_stat


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
