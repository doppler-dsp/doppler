"""Integration tests for doppler.measure.IMDMeasure (two-tone IMD/TOI)."""

from __future__ import annotations

import numpy as np

from doppler.measure import IMDMeasure

N = 4096


def _c(cyc, amp):
    return amp * np.cos(2 * np.pi * cyc * np.arange(N) / N)


def test_imd3_and_toi():
    x = _c(200, 1.0) + _c(250, 1.0) + _c(150, 0.01) + _c(300, 0.01)
    m = IMDMeasure(window="kaiser", n=N, fs=1.0, beta=12.0)
    r = m.analyze(x.astype(np.float32))
    assert abs(r.f1 - 200 / N) < 2e-3
    assert abs(r.f2 - 250 / N) < 2e-3
    assert abs(r.imd3_dbc - (-40.0)) < 0.5
    assert abs(r.imd3_lo_freq - 150 / N) < 2e-3
    assert abs(r.imd3_hi_freq - 300 / N) < 2e-3
    assert abs(r.toi_dbfs - 20.0) < 0.5  # 0 dBFS tones + |IMD3|/2


def test_imd_named_result():
    x = _c(200, 1.0) + _c(250, 1.0) + _c(150, 0.01) + _c(300, 0.01)
    m = IMDMeasure(n=N, fs=1.0, beta=12.0)
    r = m.analyze(x.astype(np.float32))
    assert type(r).__name__ == "IMDMetrics"
    assert type(r).__module__ == "doppler.measure"
    _f1, _f2, *_ = r  # unpackable
    assert isinstance(r.toi_dbfs, float)


def test_imd_defaults_construct():
    m = IMDMeasure()
    assert m.n == 8192 and m.nfft == 16384
