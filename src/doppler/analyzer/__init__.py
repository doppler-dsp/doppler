"""Spectrum analysis: Specan renders a windowed, averaged power spectral density (dBFS) over a chosen span and resolution bandwidth, mirroring a hardware spectrum analyser's span / RBW / averaging controls.

Examples
--------
>>> import numpy as np
>>> from doppler.analyzer import Specan
>>> sp = Specan(fs=1.024e6, span=200e3, rbw=2e3, src_center=0.0,
...             center=0.0, offset_db=0.0, full_scale=1.0, bits=0,
...             window="kaiser", navg=1)
>>> sp.execute(np.ones(4096, np.complex64)).shape
(201,)"""

# analyzer/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .analyzer import Specan  # noqa: E402

__all__ = ["Specan"]
