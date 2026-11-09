"""Spectral transforms: reusable FFT and FFT2D engines (fixed length, pocketfft plans cached at construction) and PSD power-spectrum estimators. The correlators are doppler.correlation's Correlator.

Examples
--------
>>> import numpy as np
>>> from doppler.spectral import FFT
>>> f = FFT(n=1024)
>>> X = f.execute_cf32(np.ones(1024, np.complex64))
>>> bool(abs(X[0].real - 1024) < 1)
True

"""

# spectral/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .spectral import (  # noqa: E402
    FFT,
    FFT2D,
    PSD,
    PSD_WINDOW,
    SPECTROGRAM_MODE,
    Spectrogram,
    blackman_harris_window,
    find_peaks_f32,
    hann_window,
    kaiser_beta_for_sidelobe,
    kaiser_enbw,
    kaiser_window,
    magnitude_db_cf32,
    magnitude_db_cf64,
    noise_floor_db,
    obw_from_power,
    power_to_db_f32,
)

__all__ = [
    "FFT",
    "FFT2D",
    "PSD",
    "kaiser_enbw",
    "kaiser_window",
    "hann_window",
    "magnitude_db_cf32",
    "magnitude_db_cf64",
    "find_peaks_f32",
    "obw_from_power",
    "noise_floor_db",
    "blackman_harris_window",
    "kaiser_beta_for_sidelobe",
    "Spectrogram",
    "PSD_WINDOW",
    "SPECTROGRAM_MODE",
    "power_to_db_f32",
]
