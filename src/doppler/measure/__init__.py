"""ADC and tone-quality metrics: ToneMeasure, NPRMeasure and IMDMeasure return named records — ENOB, SFDR, SINAD, THD and more — from a single-tone, noise-power-ratio, or two-tone intermodulation capture.

Examples
--------
>>> import numpy as np
>>> from doppler.measure import ToneMeasure
>>> tm = ToneMeasure(n=4096, fs=1.024e6)
>>> x = np.cos(2 * np.pi * 200 / 4096 * np.arange(4096)).astype(np.float32)
>>> bool(tm.analyze(x).enob > 10)
True"""

# measure/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .measure import ToneMeasure, measure_min_samples, measure_rec_nfft, measure_proc_gain, dp_coherent_freq, NPRMeasure, IMDMeasure  # noqa: E402

__all__ = ["ToneMeasure", "measure_min_samples", "measure_rec_nfft", "measure_proc_gain", "dp_coherent_freq", "NPRMeasure", "IMDMeasure"]
