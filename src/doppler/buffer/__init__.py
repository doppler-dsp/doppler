"""Sample buffering: lock-free ring buffers for handing IQ blocks between producer and consumer stages.

Examples
--------
>>> import numpy as np
>>> from doppler.buffer import F32Buffer
>>> b = F32Buffer(16)
>>> b.write(np.ones(4, np.complex64))
True
>>> b.wait(4).shape
(4,)"""

# buffer/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .buffer import F32Buffer, F64Buffer, I16Buffer  # noqa: E402

__all__ = ["F32Buffer", "F64Buffer", "I16Buffer"]
