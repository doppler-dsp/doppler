"""Spectral quality tests for doppler.resample.HalfbandDecimator."""

from __future__ import annotations

import numpy as np
import pytest

from doppler.resample import (
    HalfbandDecimator,
    _halfband_bank,
)


@pytest.fixture(scope="module")
def hb_bank():
    return _halfband_bank(60.0, 0.4, 0.6)


@pytest.fixture(scope="module")
def hb_fir(hb_bank):
    centre = hb_bank.shape[1] // 2
    fir_row = (
        0
        if abs(float(hb_bank[0, centre])) < abs(float(hb_bank[1, centre]))
        else 1
    )
    return np.ascontiguousarray(hb_bank[fir_row])


def _blackman_harris(n: int) -> np.ndarray:
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    return (
        a[0] - a[1] * np.cos(k) + a[2] * np.cos(2 * k) - a[3] * np.cos(3 * k)
    )


def _spectrum(signal: np.ndarray):
    n = len(signal)
    w = _blackman_harris(n)
    cg = w.mean()
    S = np.fft.fft(signal * w, 4 * n)
    amp_db = 20 * np.log10(np.abs(S) / (n * cg) + 1e-300)
    return np.fft.fftfreq(4 * n), amp_db


def _peak_near(bins, db, freq, tol=0.02):
    mask = np.abs(bins - freq) < tol
    return float(db[mask].max()) if mask.any() else -300.0


def _tone(freq: float, n: int) -> np.ndarray:
    t = np.arange(n, dtype=np.float64)
    return np.exp(2j * np.pi * freq * t).astype(np.complex64)


class TestHalfbandDecimatorCreate:
    def test_rate(self, hb_fir):
        r = HalfbandDecimator(hb_fir)
        assert abs(r.rate - 0.5) < 1e-9

    def test_num_taps(self, hb_fir):
        r = HalfbandDecimator(hb_fir)
        assert r.num_taps == len(hb_fir)

    def test_bad_ndim(self):
        with pytest.raises(ValueError):
            HalfbandDecimator(np.ones((2, 3, 5), dtype=np.float32))

    def test_2d_array_raises(self):
        with pytest.raises(ValueError):
            HalfbandDecimator(np.ones((4, 19), dtype=np.float32))

    def test_context_manager(self, hb_fir):
        with HalfbandDecimator(hb_fir) as r:
            y = r.execute(np.ones(64, dtype=np.complex64))
        assert y.dtype == np.complex64


class TestHalfbandDecimatorOutput:
    def test_output_dtype(self, hb_fir):
        r = HalfbandDecimator(hb_fir)
        y = r.execute(np.zeros(128, dtype=np.complex64))
        assert y.dtype == np.complex64

    def test_even_length(self, hb_fir):
        r = HalfbandDecimator(hb_fir)
        y = r.execute(np.zeros(512, dtype=np.complex64))
        assert len(y) == 256

    def test_odd_length_buffered(self, hb_fir):
        r = HalfbandDecimator(hb_fir)
        y = r.execute(np.zeros(511, dtype=np.complex64))
        assert len(y) == 255

    def test_pending_consumed_next_call(self, hb_fir):
        r = HalfbandDecimator(hb_fir)
        r.execute(np.zeros(511, dtype=np.complex64))
        y = r.execute(np.zeros(1, dtype=np.complex64))
        assert len(y) == 1

    def test_reset_restarts(self, hb_fir):
        r = HalfbandDecimator(hb_fir)
        x = _tone(0.05, 256)
        y1 = r.execute(x.copy())
        r.reset()
        y2 = r.execute(x.copy())
        n = min(len(y1), len(y2), 10)
        np.testing.assert_allclose(y1[:n], y2[:n], atol=1e-5)

    def test_stateful_across_calls(self, hb_fir):
        r1 = HalfbandDecimator(hb_fir)
        r2 = HalfbandDecimator(hb_fir)
        x = _tone(0.05, 512)
        y_full = r1.execute(x)
        y_half = np.concatenate([r2.execute(x[:256]), r2.execute(x[256:])])
        n = min(len(y_full), len(y_half))
        np.testing.assert_allclose(y_full[:n], y_half[:n], atol=1e-5)


N_IN = 8192
PASSBAND_FREQ_IN = 0.1
STOPBAND_FREQ_IN = 0.6
AMPLITUDE_TOL_DB = 0.1
ALIAS_REJECTION_DBC = 50.0


class TestHalfbandDecimatorSpectral:
    def _run(self, hb_fir, freq_in):
        r = HalfbandDecimator(hb_fir)
        x = _tone(freq_in, N_IN)
        r.execute(np.zeros(256, dtype=np.complex64))
        r.reset()
        return r.execute(x)

    def test_passband_amplitude(self, hb_fir):
        y = self._run(hb_fir, PASSBAND_FREQ_IN)
        bins, db = _spectrum(y)
        amp = _peak_near(bins, db, 0.2)
        assert abs(amp) < AMPLITUDE_TOL_DB, (
            f"passband amplitude {amp:+.3f} dB (limit ±{AMPLITUDE_TOL_DB})"
        )

    def test_passband_tone_present(self, hb_fir):
        y = self._run(hb_fir, PASSBAND_FREQ_IN)
        bins, db = _spectrum(y)
        f_meas = float(bins[np.argmax(db)])
        assert abs(f_meas - 0.2) < 0.01, (
            f"expected f_out=0.2, got {f_meas:.4f}"
        )

    def test_stopband_alias_rejected(self, hb_fir):
        y_pass = self._run(hb_fir, PASSBAND_FREQ_IN)
        y_stop = self._run(hb_fir, STOPBAND_FREQ_IN)
        _, db_pass = _spectrum(y_pass)
        _, db_stop = _spectrum(y_stop)
        peak_pass = float(db_pass.max())
        peak_stop = float(db_stop.max())
        assert peak_pass - peak_stop > ALIAS_REJECTION_DBC, (
            f"alias rejection {peak_pass - peak_stop:.1f} dBc "
            f"(limit {ALIAS_REJECTION_DBC} dBc)"
        )

    def test_state_preserved_across_odd_blocks(self, hb_fir):
        r_odd = HalfbandDecimator(hb_fir)
        r_even = HalfbandDecimator(hb_fir)
        x = _tone(PASSBAND_FREQ_IN, 512)
        y_even = r_even.execute(x)
        y_odd = np.concatenate(
            [r_odd.execute(x[:255]), r_odd.execute(x[255:])]
        )
        n = min(len(y_even), len(y_odd))
        np.testing.assert_allclose(y_even[:n], y_odd[:n], atol=1e-5)
