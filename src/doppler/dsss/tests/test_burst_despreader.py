"""Integration tests for doppler.dsss.BurstDespreader (DLL + Costas tracking).

Builds DSSS-BPSK bursts with the wfm chip mapping (0 -> +1, 1 -> -1) and checks
the despreader recovers the symbols. Phase ambiguity is don't-care (a globally
inverted decision is correct), so error rates use min(BER, 1 - BER).
"""

import numpy as np
import pytest

from doppler.dsss import BurstDespreader

SPS = 4


def _spread(code, syms):
    """Oversampled DSSS-BPSK signal: each symbol * code, rect-hold by SPS."""
    chsign = np.where(code & 1, -1.0, 1.0).astype(np.float32)
    return np.concatenate([np.repeat(s * chsign, SPS) for s in syms]).astype(
        np.complex64
    )


def _amb_ber(rec_bits, tx_syms, start=0):
    rec = np.where(rec_bits == 1, 1.0, -1.0)
    e = np.mean(rec[start:] != tx_syms[start : len(rec)])
    return min(e, 1.0 - e)


def _burst(rng, sf, n):
    code = rng.integers(0, 2, sf).astype(np.uint8)
    bits = rng.integers(0, 2, n).astype(np.uint8)
    syms = np.where(bits == 1, -1.0, 1.0).astype(np.float32)
    return code, syms, _spread(code, syms)


def test_create_validates():
    # sps < 2 and sf longer than the code are rejected (NULL -> error).
    with pytest.raises((ValueError, RuntimeError, Exception)):
        BurstDespreader(np.zeros(4, np.uint8), sf=4, sps=1)
    with pytest.raises((ValueError, RuntimeError, Exception)):
        BurstDespreader(np.zeros(2, np.uint8), sf=8, sps=2)


def test_genie_payload_zero_offset():
    rng = np.random.default_rng(0)
    code, syms, tx = _burst(rng, 31, 60)
    d = BurstDespreader(
        code, sf=31, sps=SPS, init_norm_freq=0.0, init_chip_phase=0.0
    )
    bits = d.bits(tx)
    assert len(bits) == 60
    assert _amb_ber(bits, syms) == 0.0
    assert d.lock_metric > 0.9


def test_steps_returns_complex64():
    rng = np.random.default_rng(1)
    code, syms, tx = _burst(rng, 31, 30)
    d = BurstDespreader(code, sf=31, sps=SPS)
    s = d.steps(tx)
    assert s.dtype == np.complex64
    # prompt symbol sign matches the transmitted symbol (up to global flip)
    assert _amb_ber((s.real >= 0).astype(np.uint8), syms) == 0.0


def test_seeded_carrier_offset_tracks():
    rng = np.random.default_rng(2)
    code, syms, tx = _burst(rng, 63, 300)
    f0 = 0.0006
    rx = (tx * np.exp(2j * np.pi * f0 * np.arange(len(tx)))).astype(
        np.complex64
    )
    d = BurstDespreader(
        code, sf=63, sps=SPS, init_norm_freq=f0, init_chip_phase=0.0
    )
    bits = d.bits(rx)
    assert _amb_ber(bits, syms, start=len(bits) // 4) == 0.0
    assert abs(d.norm_freq - f0) < 1e-4
    assert d.lock_metric > 0.95


def test_dll_pulls_in_code_phase():
    rng = np.random.default_rng(3)
    code, syms, tx = _burst(rng, 63, 300)
    d = BurstDespreader(
        code, sf=63, sps=SPS, init_chip_phase=0.4, bn_code=0.01
    )
    bits = d.bits(tx)
    assert _amb_ber(bits, syms, start=len(bits) // 4) == 0.0


def test_reset_reproduces():
    rng = np.random.default_rng(4)
    code, _syms, tx = _burst(rng, 31, 50)
    d = BurstDespreader(code, sf=31, sps=SPS, init_norm_freq=0.0001)
    b1 = d.bits(tx.copy())
    d.reset()
    b2 = d.bits(tx.copy())
    np.testing.assert_array_equal(b1, b2)


def test_preamble_aided_distinct_codes():
    """Realistic scenario: long acq code, 5 reps, distinct 32-chip data code.

    The unmodulated preamble pulls the loops in (set_acq), then the payload is
    despread with the data code. amb-BER 0 even with a residual carrier offset.
    """
    rng = np.random.default_rng(5)
    acq_sf, data_sf, reps = 512, 32, 5
    acq = rng.integers(0, 2, acq_sf).astype(np.uint8)
    data = rng.integers(0, 2, data_sf).astype(np.uint8)
    asig = np.where(acq & 1, -1.0, 1.0).astype(np.float32)
    bits = rng.integers(0, 2, 200).astype(np.uint8)
    syms = np.where(bits == 1, -1.0, 1.0).astype(np.float32)
    pre = np.concatenate([np.repeat(asig, SPS) for _ in range(reps)])
    pay = _spread(data, syms)
    burst = np.concatenate([pre, pay]).astype(np.complex64)
    f0 = 8e-5
    rx = (burst * np.exp(2j * np.pi * f0 * np.arange(len(burst)))).astype(
        np.complex64
    )
    d = BurstDespreader(data, sf=data_sf, sps=SPS, init_norm_freq=0.0)
    d.set_acq(acq, reps)
    out = d.bits(rx)
    # the preamble emits nothing; only the payload symbols come out
    assert len(out) == len(syms)
    assert _amb_ber(out, syms, start=len(out) // 5) == 0.0
    assert abs(d.norm_freq - f0) < 5e-5


def test_property_accessors():
    d = BurstDespreader(
        np.ones(16, np.uint8), sf=16, sps=2, init_norm_freq=0.001
    )
    # getters
    assert d.norm_freq == pytest.approx(0.001, abs=1e-6)
    for attr in (
        "bn_carrier",
        "bn_code",
        "code_phase",
        "lock_metric",
        "snr_est",
    ):
        assert isinstance(getattr(d, attr), float)
    # setters round-trip (bn setters recompute the loop gains internally)
    d.bn_carrier = 0.06
    assert d.bn_carrier == pytest.approx(0.06)
    d.bn_code = 0.02
    assert d.bn_code == pytest.approx(0.02)
    d.norm_freq = 0.0
    assert d.norm_freq == pytest.approx(0.0, abs=1e-9)


def test_context_manager_and_destroy():
    d = BurstDespreader(np.ones(8, np.uint8), sf=8, sps=2)
    with d:
        pass
    d.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        d.steps(np.zeros(16, np.complex64))


def test_steps_out_writes_into_callers_buffer():
    x = np.random.default_rng(0).standard_normal(64).astype(np.complex64)
    d = BurstDespreader(np.ones(4, np.uint8), sf=4, sps=4)
    out = np.zeros(len(x), dtype=np.complex64)
    y = d.steps(x, out=out)
    assert np.shares_memory(y, out)


def test_steps_out_undersized_raises():
    x = np.zeros(64, dtype=np.complex64)
    d = BurstDespreader(np.ones(4, np.uint8), sf=4, sps=4)
    out = np.zeros(1, dtype=np.complex64)
    with pytest.raises(ValueError):
        d.steps(x, out=out)


def test_bits_out_writes_into_callers_buffer():
    x = np.random.default_rng(0).standard_normal(64).astype(np.complex64)
    d = BurstDespreader(np.ones(4, np.uint8), sf=4, sps=4)
    out = np.zeros(len(x), dtype=np.uint8)
    y = d.bits(x, out=out)
    assert np.shares_memory(y, out)


def test_bits_out_undersized_raises():
    x = np.zeros(64, dtype=np.complex64)
    d = BurstDespreader(np.ones(4, np.uint8), sf=4, sps=4)
    out = np.zeros(1, dtype=np.uint8)
    with pytest.raises(ValueError):
        d.bits(x, out=out)
