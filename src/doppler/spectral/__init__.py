"""Spectral transforms: reusable FFT and FFT2D engines (fixed length, pocketfft plans cached at construction) and PSD power-spectrum estimators. The correlators are doppler.correlation's Correlator.

Examples
--------
>>> import numpy as np
>>> from doppler.spectral import FFT
>>> f = FFT(n=1024)
>>> X = f.execute_cf32(np.ones(1024, np.complex64))
>>> bool(abs(X[0].real - 1024) < 1)
True

The inverse is the same engine with `sign=+1`, and is unnormalised:

>>> x = np.exp(2j * np.pi * 0.1 * np.arange(1024)).astype(np.complex64)
>>> back = FFT(n=1024, sign=+1).execute_cf32(f.execute_cf32(x)) / 1024
>>> bool(np.allclose(back, x, atol=1e-4))
True

The helpers are the building blocks of a display: a window, the transform,
dB, and the peaks:

>>> from doppler.spectral import hann_window, magnitude_db_cf32, find_peaks_f32
>>> w = np.empty(1024, np.float32)
>>> hann_window(w)
>>> tone = np.exp(2j * np.pi * 100 / 1024 * np.arange(1024)).astype(np.complex64)
>>> db = magnitude_db_cf32(f.execute_cf32(tone * w), lin_floor=1e-12, offset_db=0.0)
>>> int(np.argmax(db))   # bin 100, where the tone is
100

`PSD_WINDOW` and `SPECTROGRAM_MODE` name each C enum value the `window=`
and `mode=` arguments take:

>>> from doppler.spectral import PSD_WINDOW, SPECTROGRAM_MODE
>>> sorted(PSD_WINDOW.values())
['blackman-harris', 'hann', 'kaiser', 'rect']
>>> SPECTROGRAM_MODE
{0: 'power', 1: 'db'}

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
