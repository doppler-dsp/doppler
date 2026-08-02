"""Fractional and integer sample delay: a delay line (DelayCf64) with a windowed tap view for building matched filters and correlators.

Examples
--------
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=3)
>>> for v in (1, 2, 3):
...     d.push(complex(v))
>>> d.ptr(3).real.tolist()   # most-recent-first tap snapshot
[3.0, 2.0, 1.0]"""

# delay/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .delay import DelayCf64  # noqa: E402

__all__ = ["DelayCf64"]
