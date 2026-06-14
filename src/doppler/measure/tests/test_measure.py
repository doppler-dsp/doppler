"""Integration tests for doppler.measure.ToneMeasure.

Mirrors the C-level checks at the binding layer and exercises the named
result types, the real/complex dispatch, and the headline ADC use case:
the ENOB of an ideal N-bit converter (via doppler.cvt.ADC) recovers N.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.measure import ToneMeasure


def _cos(cycles: float, amp: float, n: int = 4096) -> np.ndarray:
    return (amp * np.cos(2 * np.pi * cycles * np.arange(n) / n)).astype(
        np.float32
    )


def test_defaults_construct():
    # documented defaults must apply even though jm drops size_t defaults
    # (patched in the binding fragment).
    m = ToneMeasure()
    assert m.n == 8192
    assert m.nfft == 16384  # next_pow2(8192 * 2)
    assert m.lobe_bins > 0


@pytest.mark.parametrize("offset", [0.0, 0.25, 0.5])
def test_full_scale_tone_reads_zero_dbfs(offset):
    # window-bandwidth integration: a full-scale tone reads ~0 dBFS at any
    # sub-bin offset.
    m = ToneMeasure(n=4096, beta=12.0)
    r = m.analyze(_cos(300.0 + offset, 1.0))
    assert abs(r.fund_dbfs) < 0.1
    assert abs(r.fund_freq - (300.0 + offset) / 4096) < 2e-3


def test_thd_known_harmonics():
    m = ToneMeasure(n=4096, beta=12.0)
    x = _cos(200, 1.0) + _cos(400, 0.01)  # 2nd harmonic at -40 dBc
    r = m.analyze(x)
    assert abs(r.thd - (-40.0)) < 0.5
    assert abs(r.thd_pct - 1.0) < 0.1


def test_sfdr_nonharmonic_spur():
    m = ToneMeasure(n=4096, beta=12.0)
    x = _cos(200, 1.0) + _cos(777, 1e-3)  # -60 dBc non-harmonic spur
    r = m.analyze(x)
    assert abs(r.sfdr_dbc - 60.0) < 1.0
    assert r.worst_spur_is_harm == 0
    assert abs(r.worst_spur_freq - 777 / 4096) < 2e-3


def test_snr_white_noise():
    m = ToneMeasure(n=4096, beta=12.0)
    rng = np.random.default_rng(0)
    a, sigma = 0.5, 1e-3
    x = _cos(211, a) + (sigma * rng.standard_normal(4096)).astype(np.float32)
    r = m.analyze(x)
    expect = 10 * np.log10((a * a / 2) / (sigma * sigma))
    assert abs(r.snr - expect) < 1.5
    assert abs(r.enob - (r.sinad - 1.76) / 6.02) < 1e-6


def test_complex_negative_frequency():
    m = ToneMeasure(n=4096, beta=12.0)
    i = np.arange(4096)
    x = np.exp(-2j * np.pi * 137 * i / 4096).astype(np.complex64)
    r = m.analyze_complex(x)
    assert abs(r.fund_freq - (-137 / 4096)) < 2e-3
    assert abs(r.fund_dbfs) < 0.2


@pytest.mark.parametrize("bits", [8, 12, 14])
def test_enob_of_ideal_adc(bits):
    # The headline ADC characterisation: an ideal N-bit converter's ENOB ~= N.
    from doppler.cvt import ADC

    n = 16384
    adc = ADC(bits, 0.0, 0)  # dbfs=0 -> full scale at amplitude 1.0
    fs_amp = 2.0 ** (bits - 1)
    x = 0.999 * np.sin(2 * np.pi * 1234.567 * np.arange(n) / n)
    codes = adc.steps(x.astype(np.float32)).astype(np.float32)
    m = ToneMeasure(
        window="kaiser", n=n, beta=14.0, n_harmonics=10, full_scale=fs_amp
    )
    r = m.analyze(codes)
    assert abs(r.enob - bits) < 0.3
    assert abs(r.sinad - (6.02 * bits + 1.76)) < 1.0


def test_time_stats():
    m = ToneMeasure(n=4096)
    ts = m.time_stats(_cos(50, 0.8))
    assert abs(ts.crest_db - 3.01) < 0.1
    assert abs(ts.papr_db - ts.crest_db) < 1e-9
    assert abs(ts.fs_util_pct - 80.0) < 1.0
    assert abs(ts.dc_offset) < 1e-3


def test_named_result_fields_and_unpacking():
    m = ToneMeasure(n=4096, beta=12.0)
    r = m.analyze(_cos(300, 1.0))
    assert type(r).__name__ == "ToneMetrics"
    assert type(r).__module__ == "tonemeas"
    # attribute access and tuple unpacking both work
    assert isinstance(r.snr, float)
    assert len(tuple(r)) == 23
    assert r[7] == r.enob  # field 7 is enob


def test_spectrum_dbfs_shape():
    m = ToneMeasure(n=4096, beta=12.0)
    s = m.spectrum_dbfs(_cos(300, 1.0))
    assert s.shape == (m.nfft,)
    assert s.dtype == np.float32
    assert np.argmax(s) != 0  # peak is not at DC


def test_accuracy_metadata():
    m = ToneMeasure(n=4096, fs=1e6, beta=12.0)
    r = m.analyze(_cos(300, 1.0))
    assert r.lobe_bins == m.lobe_bins
    assert r.bin_hz == pytest.approx(1e6 / m.nfft)
    assert r.rbw_hz > r.bin_hz  # ENBW spans several bins
    assert r.n_noise_bins > 0
    assert r.floor_uncert_db > 0


def test_capture_planning_helpers():
    from math import gcd

    from doppler.measure import (
        dp_coherent_freq,
        measure_min_samples,
        measure_proc_gain,
        measure_rec_nfft,
    )

    # min_samples reaches the requested RBW once analysed at that length.
    n = measure_min_samples(1e6, 200.0, 1, 12.0)
    m = ToneMeasure(window="kaiser", n=n, fs=1e6, beta=12.0)
    assert m.rbw <= 200.0 * 1.05  # within ~5% of target

    assert measure_rec_nfft(8000, 2) == 16384
    assert measure_proc_gain(16384) == pytest.approx(10 * np.log10(8192))

    # coherent frequency: integer cycles, coprime with N, near the target.
    fsr, N = 100e6, 16384
    f = dp_coherent_freq(fsr, 10e6, N)
    j = round(f / fsr * N)
    assert gcd(j, N) == 1
    assert abs(f - 10e6) < fsr / N  # within one bin of the target
