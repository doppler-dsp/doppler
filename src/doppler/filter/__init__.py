"""FIR filtering: a direct-form complex or real FIR (FIR) and an O(1) boxcar moving average (MovingAverage).

FIR takes `float32` taps (one multiply-add per tap per sample: any
symmetric low-, high- or band-pass design) or `complex64` taps (two, plus a
permute: a Hilbert transformer, a frequency-shifted design); its input is
`complex64` either way, and its delay line carries across calls.
`design_lowpass` sizes and builds a Kaiser windowed-sinc lowpass from its
band edges (Nyquist-normalised, 1.0 == fs/2) and attenuation, with no
scipy.

Examples
--------
>>> import numpy as np
>>> from doppler.filter import MovingAverage
>>> MovingAverage(2).steps(np.ones(3, np.complex64)).real.tolist()
[0.5, 1.0, 1.0]

A designed lowpass, run on a stream in blocks:

>>> from doppler.filter import FIR, design_lowpass
>>> fir = FIR(design_lowpass(fpass=0.4, fstop=0.6, atten_db=60.0))
>>> y = np.concatenate([fir.execute(np.ones(256, np.complex64))
...                     for _ in range(4)])
>>> round(float(y[-1].real), 3)   # DC passes at unity gain
1.0

"""

# filter/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

from .filter import FIR, MovingAverage, design_lowpass  # noqa: E402

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

__all__ = ["FIR", "MovingAverage", "design_lowpass"]
