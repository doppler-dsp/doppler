"""SymbolTimingEstimator through the binding.

The object's claims are pinned in C (``native/tests/timing_est/``), where
the tolerances are derived. This file holds what only the binding can get
wrong: the record's fields, the refusal's exception, the dtype, the state
triplet and the context-manager lifecycle.
"""

import numpy as np
import pytest

from doppler.track import SymbolTimingEstimator, TimingEstimate

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
