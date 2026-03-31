"""Integration tests for doppler.resample (C-backed implementations).

These tests exercise the full path: Python-designed coefficients
(kaiser_prototype, fit_dpmfs) executed by the C library
(dp_resamp_cf32, dp_resamp_dpmfs, dp_hbdecim_cf32).  That end-to-end
round-trip is the cross-language validation — there is no separate test
harness.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.polyphase import kaiser_prototype, fit_dpmfs
from doppler.resample import HalfbandDecimator, Resampler, ResamplerDpmfs


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _tone(freq: float, n: int, dtype=np.complex64) -> np.ndarray:
    t = np.arange(n, dtype=np.float64)
    return np.exp(2j * np.pi * freq * t).astype(dtype)


def _spectrum_db(x: np.ndarray, nfft: int = 8192) -> np.ndarray:
    """Magnitude spectrum in dB, normalised to [0, 0.5) cycles/sample."""
    w = np.hanning(len(x))
    X = np.fft.fft(x.astype(np.complex128) * w, nfft)
    # keep only positive frequencies
    half = nfft // 2
    return 20 * np.log10(np.abs(X[:half]) + 1e-12)


def _peak_bin(spec: np.ndarray) -> int:
    return int(np.argmax(spec))


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def bank():
    _, b = kaiser_prototype(
        attenuation=60.0,
        passband=0.4,
        stopband=0.6,
        image_attenuation=80.0,
    )
    return b


@pytest.fixture(scope="module")
def dpmfs_coeffs(bank):
    return fit_dpmfs(bank, M=3)


# ---------------------------------------------------------------------------
# Resampler — construction
# ---------------------------------------------------------------------------


class TestResamplerCreate:
    def test_interpolation_rate(self, bank):
        r = Resampler(bank, rate=2.0)
        assert abs(r.rate - 2.0) < 1e-9

    def test_decimation_rate(self, bank):
        r = Resampler(bank, rate=0.5)
        assert abs(r.rate - 0.5) < 1e-9

    def test_bank_shape_reported(self, bank):
        r = Resampler(bank, rate=1.0)
        assert r.num_phases == bank.shape[0]
        assert r.num_taps == bank.shape[1]

    def test_bad_bank_ndim(self):
        with pytest.raises(ValueError):
            Resampler(np.ones(19, dtype=np.float32), rate=1.0)

    def test_context_manager(self, bank):
        with Resampler(bank, rate=1.5) as r:
            y = r.execute(np.ones(64, dtype=np.complex64))
        assert y.dtype == np.complex64


# ---------------------------------------------------------------------------
# Resampler — output size and dtype
# ---------------------------------------------------------------------------


class TestResamplerOutput:
    def test_output_dtype(self, bank):
        r = Resampler(bank, rate=2.0)
        y = r.execute(np.zeros(128, dtype=np.complex64))
        assert y.dtype == np.complex64

    def test_interpolation_length(self, bank):
        r = Resampler(bank, rate=2.0)
        y = r.execute(np.zeros(512, dtype=np.complex64))
        assert 900 < len(y) < 1100

    def test_decimation_length(self, bank):
        r = Resampler(bank, rate=0.5)
        y = r.execute(np.zeros(512, dtype=np.complex64))
        assert 200 < len(y) < 320

    def test_reset_restarts_phase(self, bank):
        r = Resampler(bank, rate=1.5)
        x = _tone(0.1, 256)
        y1 = r.execute(x.copy())
        r.reset()
        y2 = r.execute(x.copy())
        n = min(len(y1), len(y2), 10)
        np.testing.assert_allclose(y1[:n], y2[:n], atol=1e-5)

    def test_stateful_across_calls(self, bank):
        """Two half-block calls must equal one full-block call."""
        r1 = Resampler(bank, rate=1.5)
        r2 = Resampler(bank, rate=1.5)
        x = _tone(0.1, 512)
        y_full = r1.execute(x)
        y_half = np.concatenate([r2.execute(x[:256]), r2.execute(x[256:])])
        n = min(len(y_full), len(y_half))
        np.testing.assert_allclose(y_full[:n], y_half[:n], atol=1e-5)


# ---------------------------------------------------------------------------
# Resampler — spectral quality (Python design → C execution)
# ---------------------------------------------------------------------------


class TestResamplerSpectral:
    """Verify that kaiser_prototype() coefficients give the expected
    stopband attenuation when executed by dp_resamp_cf32."""

    N_IN = 8192

    def _run(self, bank, rate, freq_in):
        r = Resampler(bank, rate=rate)
        x = _tone(freq_in, self.N_IN)
        r.execute(np.zeros(256, dtype=np.complex64))  # warm up
        r.reset()
        return r.execute(x)

    def test_interpolation_tone_present(self, bank):
        y = self._run(bank, rate=2.0, freq_in=0.1)
        spec = _spectrum_db(y)
        expected_bin = int(0.05 * 2 * (len(spec) - 1))
        assert abs(_peak_bin(spec) - expected_bin) <= 5

    def test_interpolation_image_rejected(self, bank):
        y = self._run(bank, rate=2.0, freq_in=0.1)
        spec = _spectrum_db(y)
        peak_db = spec[_peak_bin(spec)]
        image_bin = int(0.45 * 2 * (len(spec) - 1))
        lo, hi = max(0, image_bin - 10), min(len(spec), image_bin + 10)
        image_db = float(np.max(spec[lo:hi]))
        assert peak_db - image_db > 55.0

    def test_decimation_tone_present(self, bank):
        y = self._run(bank, rate=0.5, freq_in=0.1)
        spec = _spectrum_db(y)
        expected_bin = int(0.2 * 2 * (len(spec) - 1))
        assert abs(_peak_bin(spec) - expected_bin) <= 5

    def test_decimation_alias_rejected(self, bank):
        y = self._run(bank, rate=0.5, freq_in=0.1)
        spec = _spectrum_db(y)
        peak_db = spec[_peak_bin(spec)]
        noise_db = float(np.median(spec))
        assert peak_db - noise_db > 20.0


# ---------------------------------------------------------------------------
# ResamplerDpmfs — construction
# ---------------------------------------------------------------------------


class TestResamplerDpmfsCreate:
    def test_rate(self, dpmfs_coeffs):
        r = ResamplerDpmfs(dpmfs_coeffs, rate=2.0)
        assert abs(r.rate - 2.0) < 1e-9

    def test_poly_order(self, dpmfs_coeffs):
        r = ResamplerDpmfs(dpmfs_coeffs, rate=1.0)
        assert r.poly_order == 3

    def test_num_taps(self, dpmfs_coeffs):
        r = ResamplerDpmfs(dpmfs_coeffs, rate=1.0)
        assert r.num_taps == 19


# ---------------------------------------------------------------------------
# ResamplerDpmfs — output size, dtype, statefulness
# ---------------------------------------------------------------------------


class TestResamplerDpmfsOutput:
    def test_output_dtype(self, dpmfs_coeffs):
        r = ResamplerDpmfs(dpmfs_coeffs, rate=2.0)
        y = r.execute(np.zeros(128, dtype=np.complex64))
        assert y.dtype == np.complex64

    def test_interpolation_length(self, dpmfs_coeffs):
        r = ResamplerDpmfs(dpmfs_coeffs, rate=2.0)
        y = r.execute(np.zeros(512, dtype=np.complex64))
        assert 900 < len(y) < 1100

    def test_decimation_length(self, dpmfs_coeffs):
        r = ResamplerDpmfs(dpmfs_coeffs, rate=0.5)
        y = r.execute(np.zeros(512, dtype=np.complex64))
        assert 200 < len(y) < 320

    def test_reset(self, dpmfs_coeffs):
        r = ResamplerDpmfs(dpmfs_coeffs, rate=1.5)
        x = _tone(0.1, 256)
        y1 = r.execute(x.copy())
        r.reset()
        y2 = r.execute(x.copy())
        n = min(len(y1), len(y2), 10)
        np.testing.assert_allclose(y1[:n], y2[:n], atol=1e-5)

    def test_stateful_across_calls(self, dpmfs_coeffs):
        r1 = ResamplerDpmfs(dpmfs_coeffs, rate=1.5)
        r2 = ResamplerDpmfs(dpmfs_coeffs, rate=1.5)
        x = _tone(0.1, 512)
        y_full = r1.execute(x)
        y_half = np.concatenate([r2.execute(x[:256]), r2.execute(x[256:])])
        n = min(len(y_full), len(y_half))
        np.testing.assert_allclose(y_full[:n], y_half[:n], atol=1e-5)


# ---------------------------------------------------------------------------
# Cross-implementation agreement
# ---------------------------------------------------------------------------


class TestCrossImplementation:
    """Table resampler and DPMFS resampler must agree in the passband.

    This is the end-to-end cross-language test: Python designs the
    coefficients via two different paths, the C library executes both,
    and the passband outputs must agree to within DPMFS fit tolerance.
    """

    def test_passband_agreement(self, bank, dpmfs_coeffs):
        x = _tone(0.05, 2048)
        r_table = Resampler(bank, rate=1.5)
        r_dpmfs = ResamplerDpmfs(dpmfs_coeffs, rate=1.5)
        for r in (r_table, r_dpmfs):
            r.execute(np.zeros(64, dtype=np.complex64))
            r.reset()
        y_table = r_table.execute(x)
        y_dpmfs = r_dpmfs.execute(x)
        n = min(len(y_table), len(y_dpmfs))
        skip = 30  # skip initial filter transient
        np.testing.assert_allclose(
            y_table[skip:n],
            y_dpmfs[skip:n],
            atol=5e-3,
        )


# ---------------------------------------------------------------------------
# HalfbandDecimator — fixture
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def hb_bank():
    _, b = kaiser_prototype(
        attenuation=60.0,
        passband=0.4,
        stopband=0.6,
        phases=2,
    )
    return b


# ---------------------------------------------------------------------------
# HalfbandDecimator — construction
# ---------------------------------------------------------------------------


class TestHalfbandDecimatorCreate:
    def test_rate(self, hb_bank):
        r = HalfbandDecimator(hb_bank)
        assert abs(r.rate - 0.5) < 1e-9

    def test_num_taps(self, hb_bank):
        r = HalfbandDecimator(hb_bank)
        assert r.num_taps == hb_bank.shape[1]

    def test_bad_bank_ndim(self):
        with pytest.raises(ValueError):
            HalfbandDecimator(np.ones(19, dtype=np.float32))

    def test_bad_bank_rows(self):
        with pytest.raises(ValueError):
            HalfbandDecimator(np.ones((4, 19), dtype=np.float32))

    def test_context_manager(self, hb_bank):
        with HalfbandDecimator(hb_bank) as r:
            y = r.execute(np.ones(64, dtype=np.complex64))
        assert y.dtype == np.complex64


# ---------------------------------------------------------------------------
# HalfbandDecimator — output size, dtype, statefulness
# ---------------------------------------------------------------------------


class TestHalfbandDecimatorOutput:
    def test_output_dtype(self, hb_bank):
        r = HalfbandDecimator(hb_bank)
        y = r.execute(np.zeros(128, dtype=np.complex64))
        assert y.dtype == np.complex64

    def test_even_length(self, hb_bank):
        r = HalfbandDecimator(hb_bank)
        y = r.execute(np.zeros(512, dtype=np.complex64))
        assert len(y) == 256

    def test_odd_length_buffered(self, hb_bank):
        r = HalfbandDecimator(hb_bank)
        y = r.execute(np.zeros(511, dtype=np.complex64))
        assert len(y) == 255  # 1 sample pending

    def test_pending_consumed_next_call(self, hb_bank):
        r = HalfbandDecimator(hb_bank)
        r.execute(np.zeros(511, dtype=np.complex64))
        y = r.execute(np.zeros(1, dtype=np.complex64))
        assert len(y) == 1  # pending consumed

    def test_reset_restarts(self, hb_bank):
        r = HalfbandDecimator(hb_bank)
        x = _tone(0.05, 256)
        y1 = r.execute(x.copy())
        r.reset()
        y2 = r.execute(x.copy())
        n = min(len(y1), len(y2), 10)
        np.testing.assert_allclose(y1[:n], y2[:n], atol=1e-5)

    def test_stateful_across_calls(self, hb_bank):
        """Two half-block calls must equal one full-block call."""
        r1 = HalfbandDecimator(hb_bank)
        r2 = HalfbandDecimator(hb_bank)
        x = _tone(0.05, 512)
        y_full = r1.execute(x)
        y_half = np.concatenate([r2.execute(x[:256]), r2.execute(x[256:])])
        n = min(len(y_full), len(y_half))
        np.testing.assert_allclose(y_full[:n], y_half[:n], atol=1e-5)


# ---------------------------------------------------------------------------
# HalfbandDecimator — spectral quality
# ---------------------------------------------------------------------------


class TestHalfbandDecimatorSpectral:
    """kaiser_prototype(phases=2) coefficients → C halfband decimator."""

    N_IN = 8192

    def _run(self, hb_bank, freq_in):
        r = HalfbandDecimator(hb_bank)
        x = _tone(freq_in, self.N_IN)
        r.execute(np.zeros(256, dtype=np.complex64))  # warm up
        r.reset()
        return r.execute(x)

    def test_passband_tone_present(self, hb_bank):
        """Tone at 0.1 (input rate) → 0.2 (output rate) after 2:1 decim."""
        y = self._run(hb_bank, 0.1)
        spec = _spectrum_db(y)
        expected_bin = int(0.2 * 2 * (len(spec) - 1))
        assert abs(_peak_bin(spec) - expected_bin) <= 5

    def test_stopband_alias_rejected(self, hb_bank):
        """Tone at 0.6 (input rate) is in the stopband; must be rejected."""
        y_pass = self._run(hb_bank, 0.1)
        y_stop = self._run(hb_bank, 0.6)
        peak_pass = _spectrum_db(y_pass)[_peak_bin(_spectrum_db(y_pass))]
        peak_stop = float(np.max(_spectrum_db(y_stop)))
        assert peak_pass - peak_stop > 50.0
