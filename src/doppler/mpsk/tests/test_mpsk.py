"""Integration tests for doppler.mpsk (Gray-coded M-PSK map/demap)."""

import math

import numpy as np
import pytest

from doppler.mpsk import (
    mpsk_bits_per_symbol,
    mpsk_demap,
    mpsk_diff_demap,
    mpsk_diff_map,
    mpsk_map,
)

M_ALL = (2, 4, 8)


def _qfunc(x):
    return 0.5 * math.erfc(float(x) / math.sqrt(2.0))


def test_bits_per_symbol():
    assert [mpsk_bits_per_symbol(m) for m in M_ALL] == [1, 2, 3]


@pytest.mark.parametrize("m", M_ALL)
def test_unit_amplitude(m):
    sym = np.arange(m, dtype=np.uint8)
    pts = mpsk_map(sym, m)
    assert pts.dtype == np.complex64
    assert np.allclose(np.abs(pts), 1.0, atol=1e-6)


@pytest.mark.parametrize("m", M_ALL)
def test_roundtrip_all_labels(m):
    sym = np.arange(m, dtype=np.uint8)
    assert np.array_equal(mpsk_demap(mpsk_map(sym, m), m), sym)


@pytest.mark.parametrize("m", M_ALL)
def test_gray_adjacent_one_bit(m):
    # constellation points adjacent in PHASE must differ by exactly one bit
    sym = np.arange(m, dtype=np.uint8)
    pts = mpsk_map(sym, m)
    order = np.argsort(np.angle(pts) % (2 * np.pi))
    labels = sym[order]
    for i in range(m):
        a, b = int(labels[i]), int(labels[(i + 1) % m])
        assert bin(a ^ b).count("1") == 1


@pytest.mark.parametrize("m", M_ALL)
def test_demap_uses_phase_only(m):
    # demap depends on phase, not amplitude (scale invariance)
    sym = np.arange(m, dtype=np.uint8)
    pts = mpsk_map(sym, m)
    assert np.array_equal(mpsk_demap((3.7 * pts).astype(np.complex64), m), sym)


def test_default_is_qpsk():
    sym = np.array([0, 1, 2, 3], dtype=np.uint8)
    assert np.array_equal(mpsk_map(sym), mpsk_map(sym, 4))  # m defaults to 4


@pytest.mark.parametrize("m,ebn0_db", [(2, 8.0), (4, 8.0), (8, 11.0)])
def test_ber_matches_theory(m, ebn0_db):
    # map -> complex AWGN at Eb/N0 -> demap; Gray BER must track theory
    bps = mpsk_bits_per_symbol(m)
    rng = np.random.default_rng(0)
    n = 400_000
    tx = rng.integers(0, m, n).astype(np.uint8)
    s = mpsk_map(tx, m)
    ebn0 = 10 ** (ebn0_db / 10.0)
    esn0 = ebn0 * bps  # Es = bps * Eb; symbol energy is 1
    # complex AWGN: total noise power N0 = Es/esn0 = 1/esn0, N0/2 per component
    sigma = np.sqrt(1.0 / esn0 / 2.0)
    noise = sigma * (rng.standard_normal(n) + 1j * rng.standard_normal(n))
    rx = (s + noise).astype(np.complex64)
    rxsym = mpsk_demap(rx, m)
    # bit errors = popcount(tx XOR rx) over all symbols
    xor = (tx ^ rxsym).astype(np.uint8)
    nbit_err = int(np.count_nonzero(xor[:, None] & (1 << np.arange(bps))))
    ber = nbit_err / (n * bps)
    if m <= 4:
        theory = _qfunc(np.sqrt(2 * ebn0))  # BPSK == Gray QPSK
    else:
        # 8PSK Gray high-SNR approx: SER ~ 2 Q(sqrt(2 Es/N0) sin(pi/8)),
        # BER ~ SER / bps
        theory = 2 * _qfunc(np.sqrt(2 * esn0) * np.sin(np.pi / 8)) / bps
    assert ber == pytest.approx(theory, rel=0.25)


@pytest.mark.parametrize("m", M_ALL)
def test_differential_roundtrip(m):
    rng = np.random.default_rng(1)
    sym = rng.integers(0, m, 500).astype(np.uint8)
    assert np.array_equal(mpsk_diff_demap(mpsk_diff_map(sym, m), m), sym)


@pytest.mark.parametrize("m", M_ALL)
def test_differential_phase_invariant(m):
    # a constant phase slip of any constellation step leaves every symbol after
    # the implicit-reference first one intact (the point of differential mode)
    rng = np.random.default_rng(2)
    sym = rng.integers(0, m, 500).astype(np.uint8)
    pts = mpsk_diff_map(sym, m)
    for j in range(m):
        rot = (pts * np.exp(2j * np.pi * j / m)).astype(np.complex64)
        assert np.array_equal(mpsk_diff_demap(rot, m)[1:], sym[1:])
