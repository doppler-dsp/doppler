"""Integration tests for doppler.track.SymbolSync (Gardner timing recovery)."""

import numpy as np
import pytest

from doppler.track import SymbolSync

SPS = 4
BETA = 0.35


def _rc(t, beta, T):
    t = np.asarray(t, float)
    s = np.sinc(t / T)
    denom = 1 - (2 * beta * t / T) ** 2
    cos = np.cos(np.pi * beta * t / T)
    with np.errstate(divide="ignore", invalid="ignore"):
        s = s * np.where(np.abs(denom) < 1e-8, np.pi / 4, cos / denom)
    return s


def _signal(nsym, offset=0.0, rate=1.0, snr=None, seed=0):
    rng = np.random.default_rng(seed)
    a = rng.integers(0, 2, nsym) * 2 - 1
    n = nsym * SPS
    s = np.zeros(n)
    span = 8 * SPS
    for k, ak in enumerate(a):
        c = k * SPS * rate + offset
        if c + span >= n:
            break
        idx = np.arange(max(0, int(c - span)), min(n, int(c + span)))
        s[idx] += ak * _rc(idx - c, BETA, SPS)
    s = s.astype(np.complex64)
    if snr is not None:
        p = np.sqrt(np.mean(np.abs(s) ** 2))
        std = np.sqrt(10 ** (-snr / 10)) * p
        s = s + (
            rng.normal(0, std / np.sqrt(2), n)
            + 1j * rng.normal(0, std / np.sqrt(2), n)
        ).astype(np.complex64)
    return s, a


def _ber(y, a):
    """Cross-correlation-aligned BER over a fully-locked interior window."""
    dec = np.where(y.real >= 0, 1, -1).astype(float)
    c = np.correlate(dec, a.astype(float), "full")
    k = int(np.argmax(np.abs(c)))
    lag = k - (len(a) - 1)
    inv = np.sign(c[k])
    # score the middle half of the recovered symbols (locked, valid signal)
    lo, hi = len(dec) // 4, len(dec) - len(dec) // 4
    err = cnt = 0
    for i in range(lo, hi):
        j = i - lag
        if 0 <= j < len(a):
            err += dec[i] != inv * a[j]
            cnt += 1
    return err / cnt if cnt else 1.0


def test_create_and_properties():
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    assert s.bn == pytest.approx(0.01)
    assert s.rate == pytest.approx(SPS, abs=0.1)
    s.bn = 0.02
    assert s.bn == pytest.approx(0.02)


def test_context_manager_and_destroy():
    with SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic"):
        pass
    SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="linear").destroy()


@pytest.mark.parametrize("offset", [0.0, 0.5, 1.3, 2.0, 2.7, 3.5])
def test_locks_across_timing_offsets(offset):
    x, a = _signal(1500, offset=offset, seed=1)
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    y = s.steps(x)
    assert _ber(y, a) == 0.0


@pytest.mark.parametrize("order", ["linear", "parabolic", "cubic"])
def test_all_orders_lock(order):
    x, a = _signal(1500, offset=1.7, seed=2)
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order=order)
    y = s.steps(x)
    assert _ber(y, a) == 0.0


@pytest.mark.parametrize("rate", [1.0, 1.005, 0.995])
def test_tracks_clock_rate_offset(rate):
    x, a = _signal(3000, offset=1.3, rate=rate, snr=20, seed=3)
    s = SymbolSync(sps=SPS, bn=0.005, zeta=0.707, order="cubic")
    y = s.steps(x)
    assert _ber(y, a) == 0.0
    # recovered samples/symbol tracks the true clock rate
    assert s.rate == pytest.approx(SPS * rate, abs=0.1)


def test_locks_with_noise():
    x, a = _signal(2000, offset=1.3, snr=12, seed=4)
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    y = s.steps(x)
    assert _ber(y, a) < 1e-3


def test_reset_reproducible():
    x, _ = _signal(800, offset=1.3, seed=5)
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    y1 = s.steps(x)
    s.reset()
    y2 = s.steps(x)
    assert np.array_equal(y1, y2)


def test_steps_out_writes_into_callers_buffer():
    x, _ = _signal(50, seed=5)
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    out = np.zeros(max(s.steps_max_out(), len(x)), dtype=np.complex64)
    y = s.steps(x, out=out)
    assert np.shares_memory(y, out)


def test_steps_out_undersized_raises():
    x, _ = _signal(50, seed=5)
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    with pytest.raises(ValueError):
        s.steps(x, out=np.zeros(1, dtype=np.complex64))
