"""Tests for doppler.filter.design_lowpass."""

import numpy as np
import pytest

from doppler.filter import design_lowpass
from doppler.resample import kaiser_beta, kaiser_num_taps


def _stopband_max_db(taps: np.ndarray, fstop: float, n_fft: int = 16384):
    """Return the worst-case magnitude (dB) at/above fstop (Nyquist-norm)."""
    h = np.fft.rfft(taps, n_fft)
    freqs = np.fft.rfftfreq(n_fft) * 2.0  # Nyquist-normalised (1.0 == Nyquist)
    mag_db = 20.0 * np.log10(np.abs(h) + 1e-300)
    return float(np.max(mag_db[freqs >= fstop]))


def _passband_max_dev_db(taps: np.ndarray, fpass: float, n_fft: int = 16384):
    h = np.fft.rfft(taps, n_fft)
    freqs = np.fft.rfftfreq(n_fft) * 2.0
    mag_db = 20.0 * np.log10(np.abs(h) + 1e-300)
    return float(np.max(np.abs(mag_db[freqs <= fpass])))


def test_default_args():
    taps = design_lowpass()
    assert taps.dtype == np.float32
    assert len(taps) == (kaiser_num_taps(1, 60.0, 0.2, 0.3) | 1)


def test_n_taps_matches_kaiser_num_taps():
    for fpass, fstop, atten_db in [(0.2, 0.3, 40.0), (0.1, 0.2, 80.0)]:
        taps = design_lowpass(fpass=fpass, fstop=fstop, atten_db=atten_db)
        expected = kaiser_num_taps(1, atten_db, fpass / 2.0, fstop / 2.0) | 1
        assert len(taps) == expected


def test_unit_dc_gain():
    taps = design_lowpass()
    assert taps.sum() == pytest.approx(1.0, abs=1e-3)


@pytest.mark.parametrize(
    "fpass,fstop,atten_db",
    [(0.4, 0.6, 60.0), (0.2, 0.3, 40.0), (0.1, 0.2, 80.0)],
)
def test_meets_attenuation_target(fpass, fstop, atten_db):
    taps = design_lowpass(fpass=fpass, fstop=fstop, atten_db=atten_db)
    # Kaiser sizing is an estimate — allow a couple dB of slack either way.
    assert _stopband_max_db(taps, fstop) <= -(atten_db - 2.0)


def test_passband_is_flat():
    taps = design_lowpass(fpass=0.4, fstop=0.6, atten_db=60.0)
    assert _passband_max_dev_db(taps, 0.4) < 0.5


def test_beta_uses_fir_formula_not_window_sidelobe():
    # doppler.resample.kaiser_beta (FIR design) vs
    # doppler.spectral.kaiser_beta_for_sidelobe (window sidelobe) are
    # ~13 dB apart for the same beta -- design_lowpass must be driven by
    # the former or it silently undershoots its attenuation target.
    from doppler.spectral import kaiser_beta_for_sidelobe

    atten_db = 60.0
    assert kaiser_beta(atten_db) != pytest.approx(
        kaiser_beta_for_sidelobe(atten_db)
    )
