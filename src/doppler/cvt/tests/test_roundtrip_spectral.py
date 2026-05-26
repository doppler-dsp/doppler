"""Spectral purity tests for all F32→Int→F32 roundtrip pairs.

Each encoder/decoder pair is exercised with a full-scale complex exponential.
The real and imaginary channels are quantised independently (matching how CF32
is handled throughout the pipeline).

Pass criterion: all spurious energy must be at least 80 dBc below the
fundamental.  The Q15 theoretical SNR is ~92 dB for a full-scale sinusoid,
so -80 dBc provides ~12 dB guard against measurement artefacts.

Spectral measurement:
  - N = 65536 samples, Blackman-Harris window (~-92 dB sidelobes)
  - Complex FFT (complex exponential concentrates energy at one bin)
  - Guard band of ±8 bins around the fundamental peak
"""

import numpy as np
import pytest

from doppler.cvt import (
    F32ToI16,    I16ToF32,
    F32ToI16U32, I16U32ToF32,
    F32ToI16U64, I16U64ToF32,
)

# ---------------------------------------------------------------------------
# constants
# ---------------------------------------------------------------------------

N             = 65536
FREQ_NORM     = 0.07       # cycles/sample — non-bin-aligned
SPUR_DBC_MAX  = -80.0      # dBc threshold; Q15 theory gives ~92 dB
GUARD_BINS    = 8          # half-width of main-lobe exclusion zone

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def _blackman_harris(n: int) -> np.ndarray:
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    return a[0] - a[1]*np.cos(k) + a[2]*np.cos(2*k) - a[3]*np.cos(3*k)


def _spectral_purity_dbc(x: np.ndarray) -> float:
    """Return max spurious level in dBc relative to the fundamental.

    Parameters
    ----------
    x : np.ndarray
        Complex64 array, one full-scale tone.

    Returns
    -------
    float
        Spurious peak level in dBc.  Negative means below the fundamental.
    """
    n = len(x)
    w = _blackman_harris(n)
    S = np.abs(np.fft.fft(x * w))
    peak_bin = int(np.argmax(S))
    mask = np.ones(n, dtype=bool)
    for b in range(peak_bin - GUARD_BINS, peak_bin + GUARD_BINS + 1):
        mask[b % n] = False
    peak_db = 20.0 * np.log10(S[peak_bin] + 1e-300)
    spur_db = 20.0 * np.log10(S[mask].max() + 1e-300)
    return spur_db - peak_db


def _tone() -> np.ndarray:
    t = np.arange(N, dtype=np.float64)
    return np.exp(2j * np.pi * FREQ_NORM * t).astype(np.complex64)


def _cf32_roundtrip(x: np.ndarray, enc_cls, dec_cls) -> np.ndarray:
    """Quantise each CF32 channel independently and reconstruct."""
    enc_r, dec_r = enc_cls(), dec_cls()
    enc_i, dec_i = enc_cls(), dec_cls()
    re = dec_r.steps(enc_r.steps(np.ascontiguousarray(x.real)))
    im = dec_i.steps(enc_i.steps(np.ascontiguousarray(x.imag)))
    return (re + 1j * im).astype(np.complex64)


# ---------------------------------------------------------------------------
# tests
# ---------------------------------------------------------------------------

ROUNDTRIPS = [
    pytest.param(F32ToI16,    I16ToF32,    id="I16"),
    pytest.param(F32ToI16U32, I16U32ToF32, id="I16U32"),
    pytest.param(F32ToI16U64, I16U64ToF32, id="I16U64"),
]


@pytest.mark.parametrize("enc_cls,dec_cls", ROUNDTRIPS)
def test_spectral_purity(enc_cls, dec_cls):
    """Spurious content after roundtrip must be <= -80 dBc."""
    x  = _tone()
    xq = _cf32_roundtrip(x, enc_cls, dec_cls)
    dbc = _spectral_purity_dbc(xq)
    assert dbc <= SPUR_DBC_MAX, (
        f"{enc_cls.__name__}: spurious {dbc:.1f} dBc > {SPUR_DBC_MAX} dBc"
    )
