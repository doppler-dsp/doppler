"""Digital down-conversion: complex- and real-input down-converters (DDC, Ddcr) that mix to baseband, filter, and decimate in one pass, with matched-filter flavors.

Examples
--------
>>> import numpy as np
>>> from doppler.ddc import DDC
>>> DDC(norm_freq=0.1).execute(np.ones(16, np.complex64)).size
4"""

import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .ddc import DDC, Ddcr, MatchedDDC, MatchedDdcr  # noqa: E402

__all__ = ["DDC", "Ddcr", "MatchedDDC", "MatchedDdcr"]
