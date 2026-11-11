"""ADC and tone-quality metrics: ToneMeasure, NPRMeasure and IMDMeasure return named records — ENOB, SFDR, SINAD, THD and more — from a single-tone, noise-power-ratio, or two-tone intermodulation capture.

Each component's power is integrated over its window's main lobe (IEEE Std
1241). All three analyzers take the dBFS reference as `bits` (an ADC depth,
2**(bits-1)) or `full_scale` and read it back from the shared
`doppler.spectral.PSD` core, so `bits=B` is `full_scale=2**(B-1)`, and
each exposes `spectrum_dbfs(x)`, the averaged trace its metrics come from,
for a display. `rbw` is set by the un-padded length `n`; `bin_hz` is the
zero-padded grid, which sharpens a frequency estimate but not resolution.
The free functions plan a capture: `measure_min_samples` for a target
RBW, `measure_rec_nfft`, `measure_proc_gain`, and `dp_coherent_freq` for a
leakage-free test tone.

Examples
--------
>>> import numpy as np
>>> from doppler.measure import ToneMeasure
>>> tm = ToneMeasure(n=4096, fs=1.024e6)
>>> x = np.cos(2 * np.pi * 200 / 4096 * np.arange(4096)).astype(np.float32)
>>> bool(tm.analyze(x).enob > 10)
True

"""

# measure/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .measure import (  # noqa: E402
    IMDMeasure,
    IMDMetrics,
    NPRMeasure,
    NPRMetrics,
    TimeStats,
    ToneMeasure,
    ToneMetrics,
    dp_coherent_freq,
    measure_min_samples,
    measure_proc_gain,
    measure_rec_nfft,
)

__all__ = [
    "ToneMeasure",
    "measure_min_samples",
    "measure_rec_nfft",
    "measure_proc_gain",
    "dp_coherent_freq",
    "NPRMeasure",
    "IMDMeasure",
    "ToneMetrics",
    "TimeStats",
    "NPRMetrics",
    "IMDMetrics",
]
