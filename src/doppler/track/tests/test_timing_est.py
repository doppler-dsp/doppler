"""SymbolTimingEstimator through the binding.

The object's claims are pinned in C (``native/tests/timing_est/``), where
the tolerances are derived. This file holds what only the binding can get
wrong: the record's fields, the refusal's exception, the dtype, the state
triplet and the context-manager lifecycle.
"""

import numpy as np
import pytest

from doppler.track import (
    MatchedSymbolTimingEstimator,
    SymbolTimingEstimator,
    TimingEstimate,
)

SPS = 4.3
N = 1100


def stream(tau, n0=0, n=N, period=SPS, w=0.2):
    """A stream whose power peaks ``tau`` symbols into each symbol."""
    i = np.arange(n0, n0 + n)
    p = 1.0 + 0.5 * np.cos(2 * np.pi * (i / period - tau))
    return (np.sqrt(p) * np.exp(1j * w * i)).astype(np.complex64)


def test_estimate_returns_the_record():
    e = SymbolTimingEstimator(sps=SPS).estimate(stream(0.2))
    assert isinstance(e, TimingEstimate)
    assert abs(e.tau - 0.2) < 2e-3
    assert abs(e.quality - 0.25) < 1e-2
    assert abs(e.power - 1.0) < 1e-2
    assert e.symbols == pytest.approx(N / SPS)
    assert e.has_rate == 0 and e.rate == 0.0


def test_the_carrier_does_not_enter():
    a = SymbolTimingEstimator(sps=SPS).estimate(stream(0.2, w=0.0))
    b = SymbolTimingEstimator(sps=SPS).estimate(stream(0.2, w=1.3))
    assert abs(a.tau - b.tau) < 1e-6
    # the control: a different timing is told apart at that tolerance
    c = SymbolTimingEstimator(sps=SPS).estimate(stream(0.21, w=1.3))
    assert abs(a.tau - c.tau) > 1e-3


def test_rate_from_two_calls_on_one_clock():
    eps = 1e-3
    te = SymbolTimingEstimator(sps=SPS)
    te.estimate(stream(0.1, period=SPS * (1 + eps)))
    e = te.estimate(stream(0.1, n0=N, period=SPS * (1 + eps)))
    assert e.has_rate == 1
    assert e.rate == pytest.approx(eps / (1 + eps), abs=2e-5)


def test_refuses_two_samples_a_symbol():
    with pytest.raises(ValueError, match="sps above 2"):
        SymbolTimingEstimator(sps=2.0)
    assert SymbolTimingEstimator(sps=2.5).sps == 2.5


def test_refuses_a_wrong_dtype():
    with pytest.raises((TypeError, ValueError)):
        SymbolTimingEstimator(sps=SPS).estimate(np.zeros(64, dtype=np.float64))


def test_state_resumes_between_blocks():
    a = SymbolTimingEstimator(sps=SPS)
    a.estimate(stream(0.3))
    blob = a.get_state()
    assert len(blob) == a.state_bytes()
    b = SymbolTimingEstimator(sps=SPS)
    b.set_state(blob)
    ea = a.estimate(stream(0.3, n0=N))
    eb = b.estimate(stream(0.3, n0=N))
    assert (ea.tau, ea.rate, ea.quality, ea.has_rate) == (
        eb.tau,
        eb.rate,
        eb.quality,
        eb.has_rate,
    )
    assert eb.has_rate == 1  # the previous block came across


def test_state_refuses_a_clobbered_blob():
    te = SymbolTimingEstimator(sps=SPS)
    blob = bytearray(te.get_state())
    blob[0] ^= 0xFF
    with pytest.raises(ValueError):
        te.set_state(bytes(blob))
    with pytest.raises((ValueError, TypeError)):
        te.set_state(bytes(blob[:-1]))


def test_reset_restarts_the_clock():
    te = SymbolTimingEstimator(sps=SPS)
    first = te.estimate(stream(0.3))
    te.estimate(stream(0.3, n=123))
    te.reset()
    again = te.estimate(stream(0.3))
    assert (first.tau, first.quality) == (again.tau, again.quality)
    assert again.has_rate == 0


def test_context_manager_and_destroy():
    with SymbolTimingEstimator() as te:
        assert te.sps == 4.0
    obj = SymbolTimingEstimator()
    obj.destroy()


# -- the matched flavor: its own filter, and the input's timeline ----------
#
# Alternating symbols through a root-raised cosine are one tone at half the
# symbol rate, so the stream below is what such a transmitter sends, with a
# symbol instant at sample ``n0`` and every ``sps`` after it. The C test
# (test_timing_est_core.c, section 9) holds the eye to a computed bound on
# every kind of plan; these check that the binding reaches the same object.


def alternating(sps, n0, n=4096):
    i = np.arange(n)
    return (0.25 * np.cos(np.pi * (i - n0) / sps)).astype(np.complex64)


@pytest.mark.parametrize(
    "sps,n0", [(4.0, 1.25), (8.0, 3.0), (17.333, 11.1), (64.0, 5.5)]
)
def test_matched_names_the_eye_on_the_input_timeline(sps, n0):
    te = MatchedSymbolTimingEstimator(sps=sps, pulse="rrc")
    e = te.estimate(alternating(sps, n0, n=int(600 * sps)))
    assert isinstance(e, TimingEstimate)
    miss = (e.eye - n0) / sps
    miss -= round(miss)
    # 0.013 symbols: the C test's largest computed bound over its rows
    assert abs(miss) < 0.013
    assert e.quality > 0.4
    assert te.sps == sps


def test_plain_eye_is_tau_on_the_input_timeline():
    e = SymbolTimingEstimator(sps=SPS).estimate(stream(0.3))
    miss = e.eye / SPS - e.tau
    assert abs(miss - round(miss)) < 1e-9
    assert abs(e.eye - 0.5 * N) <= 0.5 * SPS  # the one nearest the middle


@pytest.mark.parametrize(
    "kw",
    [
        {"sps": 3.9},
        {"sps": float("nan")},
        {"sps": 8.0, "beta": 1.5},
        {"sps": 8.0, "span": 0},
    ],
)
def test_matched_refuses_what_its_converter_cannot_build(kw):
    with pytest.raises(ValueError, match="MatchedSymbolTimingEstimator"):
        MatchedSymbolTimingEstimator(**kw)


def test_matched_refuses_an_unknown_pulse():
    with pytest.raises((ValueError, TypeError)):
        MatchedSymbolTimingEstimator(sps=8.0, pulse="gaussian")


def test_matched_skips_its_own_start():
    te = MatchedSymbolTimingEstimator(sps=8.0, pulse="rrc")
    x = alternating(8.0, 3.0)
    first = te.estimate(x[:64])  # inside the converter's start
    assert (first.symbols, first.power, first.quality) == (0.0, 0.0, 0.0)
    rest = te.estimate(x[64:])
    assert 0.0 < rest.symbols < (len(x) - 64) / 8.0


def test_matched_state_resumes_and_reset_restarts():
    x = alternating(5.0, 2.0)
    a = MatchedSymbolTimingEstimator(sps=5.0, pulse="rrc")
    b = MatchedSymbolTimingEstimator(sps=5.0, pulse="rrc")
    a.estimate(x[:1501])
    blob = a.get_state()
    assert len(blob) == a.state_bytes()
    b.estimate(x[:777])
    b.set_state(blob)
    ea, eb = a.estimate(x[1501:]), b.estimate(x[1501:])
    assert tuple(ea) == tuple(eb)
    # a plain estimator's blob is another object's
    with pytest.raises(ValueError):
        b.set_state(SymbolTimingEstimator(sps=4.0).get_state())
    first = MatchedSymbolTimingEstimator(sps=5.0, pulse="rrc").estimate(
        x[:2001]
    )
    a.reset()
    assert tuple(a.estimate(x[:2001])) == tuple(first)
