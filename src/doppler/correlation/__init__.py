"""Correlation: one class, Correlator, for every FFT correlator in the library
-- a 1-D or 2-D map, a delay-Doppler map, a streaming detector, and the
continuous and burst code-phase x Doppler searches (also called acquisition
when part of a detector or receiver). Its knobs pick the flavor; its flavor
property says which one they built. docs/design/correlator-python.md is the
design.

Examples
--------
>>> import numpy as np
>>> from doppler.correlation import Correlator
>>> ref = np.zeros(8, np.complex64); ref[0] = 1.0
>>> c = Correlator(ref)
>>> c.flavor
'1d'

"""

# correlation/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .correlation import CORRELATOR_FLAVOR, Correlator  # noqa: E402

__all__ = ["Correlator", "CORRELATOR_FLAVOR"]
