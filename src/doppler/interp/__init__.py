"""Interpolation: a table-driven resampling interpolator (InterpolatedTable) for fractional-delay and rate-change lookups.

Examples
--------
>>> import numpy as np
>>> from doppler.interp import InterpolatedTable
>>> t = InterpolatedTable(np.arange(4, dtype=np.complex128))
>>> t.execute(np.array([1.5], np.float64)).real.round(2).tolist()
[1.5]"""

# interp/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .interp import InterpolatedTable  # noqa: E402

__all__ = ["InterpolatedTable"]
