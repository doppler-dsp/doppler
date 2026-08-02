"""Shared numeric utilities used across the doppler modules.

Examples
--------
>>> from doppler.util import square_clip
>>> square_clip(2 + 0j, 1.0)
(1+0j)"""

# util/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .util import square_clip  # noqa: E402

__all__ = ["square_clip"]
