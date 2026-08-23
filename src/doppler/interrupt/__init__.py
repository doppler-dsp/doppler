"""Stopping a run: the one flag every blocking wait in doppler consults, and the scoped guard that arms it.

Examples
--------
>>> from doppler.interrupt import Interrupt
>>> it = Interrupt([])
>>> it.interrupted()
0"""

# interrupt/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .interrupt import Interrupt  # noqa: E402

__all__ = ["Interrupt"]
