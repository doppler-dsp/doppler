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
    mpsk_soft_demap,
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


# ── soft demapping ────────────────────────────────────────────────────────
#
# The C test owns the properties (sign vs mpsk_demap, the closed forms,
# linearity in n0, confidence, the refusals). What only the binding can be
# wrong about is the binding: the out-param shape, the dtype it demands, and
# whether the default `m` matches its hard sibling's.


@pytest.mark.parametrize("m", M_ALL)
def test_soft_demap_sign_is_the_hard_decision(m):
    # The one property worth restating on this side too, because it is the
    # one a caller relies on: the soft path decides what mpsk_demap decides.
    bps = mpsk_bits_per_symbol(m)
    rng = np.random.default_rng(11)
    n = 20_000
    tx = rng.integers(0, m, n).astype(np.uint8)
    n0 = 10 ** (-3.0 / 10.0)  # a deliberately bad 3 dB, near the coin toss
    noise = np.sqrt(n0 / 2) * (
        rng.standard_normal(n) + 1j * rng.standard_normal(n)
    )
    rx = (mpsk_map(tx, m) + noise).astype(np.complex64)

    llr = np.empty(n * bps, dtype=np.float32)
    mpsk_soft_demap(rx, llr, m, n0)

    bits = (llr < 0).astype(np.uint8).reshape(n, bps)
    decided = np.zeros(n, dtype=np.uint8)
    for b in range(bps):
        decided |= bits[:, b] << b
    assert np.array_equal(decided, mpsk_demap(rx, m))


def test_soft_demap_writes_the_whole_out_param():
    # x_len * bps, not x_len -- the shape the out-param exists for, and the
    # one a caller sizing from len(x) would get wrong.
    x = np.array([1 + 0j, 1j, -1 + 0j], dtype=np.complex64)
    llr = np.full(3 * 3, np.nan, dtype=np.float32)
    mpsk_soft_demap(x, llr, 8, 1.0)
    assert np.isfinite(llr).all()


def test_soft_demap_default_m_matches_its_hard_sibling():
    x = np.array([0.7 + 0.7j, -0.6 + 0.5j], dtype=np.complex64)
    a = np.empty(4, dtype=np.float32)
    b = np.empty(4, dtype=np.float32)
    mpsk_soft_demap(x, a)  # m defaults to 4, like mpsk_demap
    mpsk_soft_demap(x, b, 4)
    assert np.array_equal(a, b)


# ── QAM, beside the PSK maps ───────────────────────────────────────────────
#
# The ALGORITHM is pinned in C (native/tests/mpsk/test_mpsk_core.c, section
# 8). What only Python can see is the door: the 16-bit label dtype (1024-QAM
# is ten bits, so a uint8 label cannot carry it), that the module exports the
# functions, and that the binding passes the mapping through unchanged.

QAM_M = (4, 16, 64, 256, 1024)


def test_qam_bits_per_symbol():
    from doppler.mpsk import qam_bits_per_symbol

    assert [qam_bits_per_symbol(m) for m in QAM_M] == [2, 4, 6, 8, 10]
    assert qam_bits_per_symbol(8) == 0  # an M-PSK order is not a QAM order


@pytest.mark.parametrize("m", QAM_M)
def test_qam_roundtrip_all_labels_and_unit_average_energy(m):
    from doppler.mpsk import qam_demap, qam_map

    lab = np.arange(m, dtype=np.uint16)
    pts = qam_map(lab, m)
    assert pts.dtype == np.complex64
    np.testing.assert_array_equal(qam_demap(pts, m), lab)
    assert float(np.mean(np.abs(pts) ** 2)) == pytest.approx(1.0, abs=1e-5)


def test_qam_labels_past_a_byte_survive():
    """1024-QAM labels run to 1023: a byte-wide binding would alias them."""
    from doppler.mpsk import qam_demap, qam_map

    lab = np.array([0, 255, 256, 511, 512, 1023], dtype=np.uint16)
    np.testing.assert_array_equal(qam_demap(qam_map(lab, 1024), 1024), lab)


def test_qam_4_is_the_librarys_qpsk():
    from doppler.mpsk import qam_map

    lab = np.arange(4, dtype=np.uint8)
    np.testing.assert_allclose(
        qam_map(lab.astype(np.uint16), 4), mpsk_map(lab, 4), atol=1e-6
    )


def test_qam_decision_is_nearest_point_not_phase():
    """Unlike M-PSK, amplitude decides: the same phase, two radii, two
    labels."""
    from doppler.mpsk import qam_demap

    s = 1.0 / math.sqrt(10.0)
    inner = np.array([1 * s + 1j * s], dtype=np.complex64)
    outer = np.array([3 * s + 3j * s], dtype=np.complex64)
    assert qam_demap(inner, 16)[0] != qam_demap(outer, 16)[0]
