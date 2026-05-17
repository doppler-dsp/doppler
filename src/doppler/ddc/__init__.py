"""ddc — Digital Down-Converter types."""

import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .ddc import DDC, DDCR  # noqa: E402

__all__ = ["DDC", "DDCR"]
