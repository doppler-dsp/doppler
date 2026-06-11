"""Integration tests for the Synth waveform engine.

Covers the five waveform types, smart defaults (bare construct → clean tone),
the string ``type``/``snr_mode`` choices, MLS PN behaviour, and the
auto-resolved SNR (Es/No for the modulated types).
"""

import numpy as np
import pytest

from doppler.wfm import Synth


def _power(x):
    return float(np.mean(np.abs(x) ** 2))


def test_default_is_clean_tone():
    """Bare construct → a clean baseband tone (smart defaults)."""
    x = Synth().steps(4096)
    assert x.dtype == np.complex64
    assert np.isclose(_power(x), 1.0, atol=0.05)  # unit-power tone, ~no noise


def test_tone_freq_peak():
    x = Synth(type="tone", fs=1e6, freq=100_000, snr=100).steps(4096)
    peak = np.argmax(np.abs(np.fft.fft(x * np.hanning(len(x))))) / len(x)
    assert abs(peak - 0.1) < 0.01


def test_noise_unit_power():
    n = Synth(type="noise", seed=7).steps(8192)
    assert np.isclose(_power(n), 1.0, atol=0.1)


def test_pn_is_maximal_length():
    """Length-7 PN with sps=1 is a balanced, period-127 m-sequence."""
    p = Synth(type="pn", pn_length=7, sps=1, snr=100).steps(127 * 2)
    chips = np.sign(p.real).astype(int)
    assert np.array_equal(chips[:127], chips[127:254])  # period 127
    assert int(np.sum(chips[:127] == -1)) == 64  # 64 ones / 63 zeros


def test_pn_fibonacci_differs_from_galois():
    """--lfsr selects the realization: same MLS period, different chips."""
    g = Synth(type="pn", pn_length=9, sps=1, snr=100, lfsr="galois")
    f = Synth(type="pn", pn_length=9, sps=1, snr=100, lfsr="fibonacci")
    gc = np.sign(g.steps(511 * 2).real).astype(int)
    fc = np.sign(f.steps(511 * 2).real).astype(int)
    assert np.array_equal(fc[:511], fc[511:1022])  # fibonacci also maximal
    assert int(np.sum(fc[:511] == -1)) == 256  # balanced (2**8)
    assert not np.array_equal(gc, fc)  # distinct ordering


def test_pn_64bit_length():
    """pn_length > 32 drives the 64-bit LFSR + auto-MLS table (n=2..64)."""
    x = Synth(type="pn", pn_length=40, sps=1, snr=100).steps(20000)
    chips = np.sign(x.real).astype(int)
    assert set(chips.tolist()) <= {-1, 1}  # clean ±1 chips
    assert abs(chips.mean()) < 0.05  # balanced over a 2**40-1 sequence


def test_bpsk_constellation():
    b = Synth(type="bpsk", sps=8, snr=100, freq=0).steps(8 * 64)
    centers = b[4::8]
    assert set(np.sign(centers.real).astype(int)) == {-1, 1}
    assert np.allclose(centers.imag, 0, atol=1e-3)


def test_qpsk_constellation():
    q = Synth(type="qpsk", sps=8, snr=100, freq=0).steps(8 * 64)
    c = q[4::8]
    assert np.isclose(np.mean(np.abs(c)), 1.0, atol=0.02)
    quad = {(int(np.sign(s.real)), int(np.sign(s.imag))) for s in c}
    assert len(quad) == 4


def test_bpsk_esno_auto_snr():
    """--snr on bpsk auto-resolves to Es/No over the data symbols."""
    s = Synth(type="bpsk", sps=8, snr=10, seed=3).steps(1 << 16)
    snr_fs = 10 - 10 * np.log10(8)  # Es/No 10 dB spread over sps=8 samples
    expected_total = 1 + 1 / (10 ** (snr_fs / 10))
    assert np.isclose(_power(s), expected_total, atol=0.05)


def test_reset_reproduces():
    obj = Synth(type="qpsk", sps=4, seed=11)
    a = obj.steps(512)
    obj.reset()
    assert np.array_equal(a, obj.steps(512))


def test_bad_type_rejected():
    with pytest.raises((ValueError, TypeError)):
        Synth(type="not-a-waveform")
