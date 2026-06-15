# spectral/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .spectral import FFT, FFT2D, Corr, Corr2D, Detector, Detector2D, PSD, kaiser_enbw, kaiser_window, hann_window, magnitude_db_cf32, magnitude_db_cf64, find_peaks_f32, obw_from_power, noise_floor_db  # noqa: E402

__all__ = ["FFT", "FFT2D", "Corr", "Corr2D", "Detector", "Detector2D", "PSD", "kaiser_enbw", "kaiser_window", "hann_window", "magnitude_db_cf32", "magnitude_db_cf64", "find_peaks_f32", "obw_from_power", "noise_floor_db"]
