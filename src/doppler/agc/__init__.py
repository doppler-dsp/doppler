"""Automatic gain control: a log-domain feedback AGC (AGC) that drives signal power toward a reference level.

Examples
--------
>>> import numpy as np
>>> from doppler.agc import AGC
>>> y = AGC(ref_db=0.0, loop_bw=0.05).steps(0.1 * np.ones(200, np.complex64))
>>> bool(abs(y[-1]) > abs(y[0]))
True"""

# agc/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .agc import AGC, settling_samples  # noqa: E402

__all__ = ["AGC", "settling_samples"]
