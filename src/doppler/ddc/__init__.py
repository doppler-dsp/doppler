"""ddc — Digital Down-Converter types."""

import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .ddc import DDC  # noqa: E402
from .ddc_fn import Ddcr  # noqa: E402

__all__ = ["DDC", "Ddcr"]
