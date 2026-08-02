"""FIR filtering: a direct-form complex or real FIR (FIR) and an O(1) boxcar moving average (MovingAverage).

Examples
--------
>>> import numpy as np
>>> from doppler.filter import MovingAverage
>>> MovingAverage(2).steps(np.ones(3, np.complex64)).real.tolist()
[0.5, 1.0, 1.0]"""

# filter/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

from .filter import FIR, MovingAverage, design_lowpass  # noqa: E402

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

__all__ = ["FIR", "MovingAverage", "design_lowpass"]
