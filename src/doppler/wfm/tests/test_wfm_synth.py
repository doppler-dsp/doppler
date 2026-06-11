"""Integration tests for the Synth waveform engine.

Covers the six waveform types, smart defaults (bare construct → clean tone),
the string ``type``/``snr_mode`` choices, MLS PN behaviour, the LFM chirp
sweep, and the auto-resolved SNR (Es/No for the modulated types).
"""

import numpy as np
import pytest

from doppler.wfm import Synth, chirp


def _inst_freq(x, fs):
    """Per-sample instantaneous frequency (Hz) from the phase increment."""
    return np.angle(x[1:] * np.conj(x[:-1])) / (2 * np.pi) * fs


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


# ── chirp (LFM) ──────────────────────────────────────────────────────────────


def test_chirp_unit_envelope():
    """A pure FM sweep has constant (unit) magnitude everywhere."""
    x = chirp(f_start=1e5, f_end=3e5, fs=1e6).steps(4096)
    assert np.allclose(np.abs(x), 1.0, atol=1e-4)


def test_chirp_up_sweep_linear():
    """Instantaneous frequency rises linearly from f_start to f_end."""
    fs, f0, f1, n = 1e6, 1e5, 3e5, 8192
    x = chirp(f_start=f0, f_end=f1, fs=fs).steps(n)
    f = _inst_freq(x, fs)
    assert np.isclose(f[0], f0, atol=2e3)  # starts at f_start
    assert np.isclose(f[-1], f1, atol=2e3)  # ends at f_end
    # linear ramp: a straight-line fit explains essentially all the variance
    coeffs = np.polyfit(np.arange(len(f)), f, 1)
    resid = f - np.polyval(coeffs, np.arange(len(f)))
    assert coeffs[0] > 0 and np.std(resid) < 2e3


def test_chirp_down_sweep():
    """f_end < f_start sweeps high → low."""
    fs, f0, f1, n = 1e6, 3e5, 1e5, 8192
    f = _inst_freq(chirp(f_start=f0, f_end=f1, fs=fs).steps(n), fs)
    assert np.isclose(f[0], f0, atol=2e3)
    assert np.isclose(f[-1], f1, atol=2e3)


def test_chirp_span_is_generation_length():
    """The sweep fills exactly the requested length: f_end is hit at sample N."""
    fs, f0, f1 = 1e6, 1e5, 4e5
    short = _inst_freq(chirp(f_start=f0, f_end=f1, fs=fs).steps(2000), fs)
    long = _inst_freq(chirp(f_start=f0, f_end=f1, fs=fs).steps(8000), fs)
    # both reach f_end at their own end, so the short sweep ramps ~4x faster
    assert np.isclose(short[-1], f1, atol=3e3)
    assert np.isclose(long[-1], f1, atol=3e3)
    assert (short[1] - short[0]) > 3 * (long[1] - long[0])


def test_chirp_reset_reproduces():
    s = chirp(f_start=1e5, f_end=3e5, fs=1e6)
    a = s.steps(4096)
    s.reset()
    assert np.array_equal(a, s.steps(4096))


def test_chirp_respects_snr():
    """A noisy chirp adds AWGN over fs like a tone (clean chirp is unit power)."""
    clean = chirp(f_start=1e5, f_end=3e5, fs=1e6, snr=100).steps(1 << 16)
    noisy = chirp(f_start=1e5, f_end=3e5, fs=1e6, snr=10, seed=3).steps(
        1 << 16
    )
    assert np.isclose(_power(clean), 1.0, atol=0.02)
    # signal (1) + noise (1/10) over the band
    assert np.isclose(_power(noisy), 1 + 1 / 10, atol=0.05)


def test_chirp_freq_is_f_start_alias():
    """``freq`` and ``f_start`` are the same knob for a chirp."""
    a = Synth(type="chirp", freq=1e5, f_end=2e5, fs=1e6).steps(1024)
    b = Synth(type="chirp", f_start=1e5, f_end=2e5, fs=1e6).steps(1024)
    assert np.array_equal(a, b)
