"""Running accumulators: single- and double-precision complex scalar sums (AccF32, AccCf64) and a per-tap trace accumulator (AccTrace), each carrying a running total across calls.

Examples
--------
>>> from doppler.accumulator import AccF32
>>> a = AccF32()
>>> a.step(1.5); a.step(2.5)
>>> a.get()
4.0"""

# accumulator/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .accumulator import AccF32, AccCf64, AccTrace  # noqa: E402

__all__ = ["AccF32", "AccCf64", "AccTrace"]
