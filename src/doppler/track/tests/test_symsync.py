"""Integration tests for doppler.track.SymbolSync (Gardner/DTTL timing
recovery)."""

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


@pytest.mark.parametrize("ted", ["gardner", "dttl"])
def test_all_teds_lock(ted):
    # DTTL's sign() decision device is only valid for BPSK/QPSK-like
    # independent I/Q rails; _signal() is real-valued BPSK so both TEDs
    # are in their valid domain here.
    x, a = _signal(1500, offset=2.1, seed=6)
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, ted=ted)
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


def _signal_safe_prefix(nsym, margin=100, **kwargs):
    """_signal()'s pulse truncates near the very end of its buffer (no room
    left for the tail once the symbol's span would run past the array) --
    reading final state after feeding the *whole* buffer would read a
    corrupted, not steady-state, value (found the hard way calibrating the
    C core's own tests). Generate `margin` extra symbols and only feed the
    safe prefix, well clear of the truncated tail (`span/SPS` = 8 symbols).
    """
    x, a = _signal(nsym + margin, **kwargs)
    return x[: nsym * SPS], a


def test_lock_defaults_unlocked():
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    assert s.locked is False
    assert s.lock_stat == 0.0


def test_lock_acquires_on_clean_signal():
    # Gardner-style eye-opening ratio, block-averaged over the default-
    # derived avgs looks (rolloff=0.35, esno_min=10dB, pfa=1e-3, pd=0.9):
    # clean RC-shaped BPSK settles well above the ~0.24 declare threshold.
    x = _signal_safe_prefix(3000, offset=1.3, snr=20, seed=7)[0]
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    s.steps(x)
    assert s.locked is True
    assert s.lock_stat > 0.4


def test_lock_stays_low_on_noise():
    rng = np.random.default_rng(8)
    n = 3000 * SPS
    x = (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
        np.complex64
    )
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    s.steps(x)
    assert s.locked is False


def test_configure_lock_rejects_bad_pfa_pd():
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    with pytest.raises(ValueError):
        s.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=0.0, pd=0.9)
    with pytest.raises(ValueError):
        s.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=1.0, pd=0.9)
    with pytest.raises(ValueError):
        # pd must exceed pfa
        s.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=0.9, pd=0.9)


def test_configure_lock_raw_unreachable_threshold_never_locks():
    x = _signal_safe_prefix(3000, offset=1.3, snr=20, seed=7)[0]
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    s.configure_lock_raw(
        avgs=20, up_thresh=2.0, down_thresh=1.9, n_up=1, n_down=1
    )
    s.steps(x)
    assert s.locked is False


def test_configure_lock_raw_low_threshold_locks_fast():
    # n_up=1 with an easily-reachable threshold declares on the first
    # above-threshold block once the loop has acquired.
    x = _signal_safe_prefix(3000, offset=1.3, snr=20, seed=7)[0]
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    s.configure_lock_raw(
        avgs=20, up_thresh=0.05, down_thresh=0.02, n_up=1, n_down=32
    )
    s.steps(x)
    assert s.locked is True


def test_lock_reset_clears():
    x = _signal_safe_prefix(3000, offset=1.3, snr=20, seed=7)[0]
    s = SymbolSync(sps=SPS, bn=0.01, zeta=0.707, order="cubic")
    s.steps(x)
    assert s.locked is True
    s.reset()
    assert s.locked is False
    assert s.lock_stat == 0.0
