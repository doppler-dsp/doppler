"""Integration tests for doppler.measure.NPRMeasure (notched-noise NPR)."""

from __future__ import annotations

import numpy as np
import pytest

from doppler.measure import NPRMeasure


def _notched_noise(n, depth_db, seed=0):
    """Broadband noise over [0.05,0.45] with a notch at [0.20,0.25]."""
    rng = np.random.default_rng(seed)
    F = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n)
    F[(f < 0.05) | (f > 0.45)] = 0
    F[(f >= 0.20) & (f <= 0.25)] *= 10 ** (depth_db / 20)
    x = np.fft.irfft(F, n)
    return (0.3 * x / np.std(x)).astype(np.float32)


@pytest.mark.parametrize("depth", [40, 55, 70])
def test_npr_tracks_notch_depth(depth):
    n = 1 << 15
    x = _notched_noise(n, -depth)
    m = NPRMeasure(n=n, fs=1.0, dynamic_range_db=90.0)
    r = m.analyze(x, 0.05, 0.45, 0.20, 0.25, 0.01)
    assert abs(r.npr_db - depth) < 5.0
    assert r.n_inband_bins > 0 and r.n_notch_bins > 0
    assert r.notch_psd_dbfs < r.inband_psd_dbfs


def test_npr_named_result():
    n = 1 << 14
    x = _notched_noise(n, -50)
    m = NPRMeasure(n=n, fs=1.0, dynamic_range_db=90.0)
    r = m.analyze(x, 0.05, 0.45, 0.20, 0.25, 0.01)
    assert type(r).__name__ == "NPRMetrics"
    assert type(r).__module__ == "doppler.measure"
    assert isinstance(r.npr_db, float)
    _npr, *_ = r  # unpackable


def test_npr_defaults_construct():
    m = NPRMeasure()  # jm#244 size_t defaults applied by hand
    assert m.n == 8192
    assert m.nfft == 16384


def test_spectrum_dbfs_out_writes_into_callers_buffer():
    n = 1 << 14
    x = _notched_noise(n, -50)
    m = NPRMeasure(n=n, fs=1.0, dynamic_range_db=90.0)
    out = np.zeros(max(m.spectrum_dbfs_max_out(), len(x)), dtype=np.float32)
    y = m.spectrum_dbfs(x, out=out)
    assert np.shares_memory(y, out)


def test_spectrum_dbfs_out_undersized_raises():
    n = 1 << 14
    x = _notched_noise(n, -50)
    m = NPRMeasure(n=n, fs=1.0, dynamic_range_db=90.0)
    with pytest.raises(ValueError):
        m.spectrum_dbfs(x, out=np.zeros(1, dtype=np.float32))
