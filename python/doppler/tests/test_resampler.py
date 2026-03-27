"""Spectral quality tests for the Python reference resampler.

Interpolator (r=2.0333): two tones at 0.1·Fin and 0.4·Fin.
  Both are in the passband and must appear unmolested in the output.

Decimator (r=0.50333): two tones at 0.4·Fout and 0.6·Fout.
  Tone 1 (0.4·Fout) lands in the passband and must appear unmolested.
  Tone 2 (0.6·Fout) is above Nyquist_out; the anti-alias filter must
  reject it before it can fold back onto tone 1.

Each test verifies:
  - output tone appears at the correct frequency
  - amplitude change < 0.1 dB
  - all spectral artifacts < −60 dBc
"""

import numpy as np
import pytest

from doppler.resample.reference import Resampler


# ---------------------------------------------------------------------------
# Test parameters
# ---------------------------------------------------------------------------

N_IN             = 8192

R_INTERP         = 2.0333
FREQS_INTERP     = [0.1, 0.4]      # normalised to Fin

R_DECIM          = 0.50333
FREQS_DECIM_IN   = [0.4 * R_DECIM, 0.6 * R_DECIM]   # in Fin units

AMPLITUDE_TOL_DB = 0.1
ARTIFACT_DB      = -60


# ---------------------------------------------------------------------------
# Spectral helpers
# ---------------------------------------------------------------------------

def _blackman_harris(n):
    """4-term Blackman-Harris window (Nuttall 1981)."""
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    return a[0] - a[1]*np.cos(k) + a[2]*np.cos(2*k) - a[3]*np.cos(3*k)


def _spectrum(signal):
    """4× zero-padded Blackman-Harris FFT.

    Returns (bins, amplitude_db) where a unit-amplitude complex tone
    reads 0 dBFS.
    """
    n  = len(signal)
    w  = _blackman_harris(n)
    cg = w.mean()
    S  = np.fft.fft(signal * w, 4 * n)
    amp_db = 20 * np.log10(np.abs(S) / (n * cg) + 1e-300)
    return np.fft.fftfreq(4 * n), amp_db


def _wrap(f):
    """Wrap frequency to (−0.5, 0.5] (fftfreq convention)."""
    return (f + 0.5) % 1.0 - 0.5


def _peak_near(bins, db, freq, tol=0.02):
    """Peak dB within ±tol of freq."""
    mask = np.abs(bins - freq) < tol
    return float(db[mask].max()) if mask.any() else -300.0


# ---------------------------------------------------------------------------
# Interpolator tests  (r = 2.0333)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("f_in", FREQS_INTERP)
def test_interp_passband_amplitude(f_in):
    """Tone amplitude change through interpolator is < AMPLITUDE_TOL_DB."""
    t = np.arange(N_IN)
    x = np.exp(2j * np.pi * f_in * t)
    y = Resampler(r=R_INTERP).execute(x)

    f_out = _wrap(f_in / R_INTERP)
    bins, db = _spectrum(y)
    amp_db = _peak_near(bins, db, f_out)

    assert abs(amp_db) < AMPLITUDE_TOL_DB, (
        f"interp r={R_INTERP}, f_in={f_in}: amplitude {amp_db:+.3f} dB "
        f"(limit ±{AMPLITUDE_TOL_DB} dB)"
    )


@pytest.mark.parametrize("f_in", FREQS_INTERP)
def test_interp_output_frequency(f_in):
    """Interpolator output tone appears at f_in / r."""
    t = np.arange(N_IN)
    x = np.exp(2j * np.pi * f_in * t)
    y = Resampler(r=R_INTERP).execute(x)

    f_out_expected = _wrap(f_in / R_INTERP)
    bins, db = _spectrum(y)
    peak_idx   = np.argmax(db)
    f_out_meas = float(bins[peak_idx])

    assert abs(f_out_meas - f_out_expected) < 0.01, (
        f"interp r={R_INTERP}, f_in={f_in}: expected f_out={f_out_expected:.4f}, "
        f"got {f_out_meas:.4f}"
    )


def test_interp_spectral_artifacts():
    """Interpolator: spurious content < ARTIFACT_DB dBc."""
    t = np.arange(N_IN)
    x = sum(np.exp(2j * np.pi * f * t) for f in FREQS_INTERP)
    y = Resampler(r=R_INTERP).execute(x)

    bins, db = _spectrum(y)
    sig_peak  = db.max()

    mask = np.ones(len(bins), dtype=bool)
    for f_in in FREQS_INTERP:
        f_out = _wrap(f_in / R_INTERP)
        mask &= np.abs(bins - f_out) > 0.02

    artifact_dbc = float(db[mask].max()) - sig_peak
    assert artifact_dbc < ARTIFACT_DB, (
        f"interp r={R_INTERP}: artifacts {artifact_dbc:.1f} dBc "
        f"(limit {ARTIFACT_DB} dBc)"
    )


# ---------------------------------------------------------------------------
# Decimator tests  (r = 0.50333)
# ---------------------------------------------------------------------------

def test_decim_passband_amplitude():
    """Tone 1 amplitude change through decimator is < AMPLITUDE_TOL_DB."""
    f_in = FREQS_DECIM_IN[0]          # 0.4·Fout expressed in Fin units
    t = np.arange(N_IN)
    x = np.exp(2j * np.pi * f_in * t)
    y = Resampler(r=R_DECIM).execute(x)

    f_out = _wrap(f_in / R_DECIM)     # should be 0.4
    bins, db = _spectrum(y)
    amp_db = _peak_near(bins, db, f_out)

    assert abs(amp_db) < AMPLITUDE_TOL_DB, (
        f"decim r={R_DECIM}, f_in={f_in:.4f}: amplitude {amp_db:+.3f} dB "
        f"(limit ±{AMPLITUDE_TOL_DB} dB)"
    )


def test_decim_output_frequency():
    """Decimator tone 1 output appears at 0.4 (Fout-normalised)."""
    f_in = FREQS_DECIM_IN[0]
    t = np.arange(N_IN)
    x = np.exp(2j * np.pi * f_in * t)
    y = Resampler(r=R_DECIM).execute(x)

    f_out_expected = _wrap(f_in / R_DECIM)
    bins, db = _spectrum(y)
    peak_idx   = np.argmax(db)
    f_out_meas = float(bins[peak_idx])

    assert abs(f_out_meas - f_out_expected) < 0.01, (
        f"decim r={R_DECIM}, f_in={f_in:.4f}: expected f_out={f_out_expected:.4f}, "
        f"got {f_out_meas:.4f}"
    )


def test_decim_spectral_artifacts():
    """Decimator: tone 2 alias rejected, all artifacts < ARTIFACT_DB dBc."""
    t = np.arange(N_IN)
    x = sum(np.exp(2j * np.pi * f * t) for f in FREQS_DECIM_IN)
    y = Resampler(r=R_DECIM).execute(x)

    bins, db = _spectrum(y)
    sig_peak  = db.max()

    # Exclude ±0.02 around tone 1 output frequency only
    f_out_tone1 = _wrap(FREQS_DECIM_IN[0] / R_DECIM)
    mask = np.abs(bins - f_out_tone1) > 0.02

    artifact_dbc = float(db[mask].max()) - sig_peak
    assert artifact_dbc < ARTIFACT_DB, (
        f"decim r={R_DECIM}: artifacts {artifact_dbc:.1f} dBc "
        f"(limit {ARTIFACT_DB} dBc)"
    )
