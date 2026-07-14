# filter/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

from .filter import FIR, MovingAverage, design_lowpass  # noqa: E402

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

__all__ = ["FIR", "MovingAverage", "design_lowpass"]
