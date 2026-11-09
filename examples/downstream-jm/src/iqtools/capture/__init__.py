# capture/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .capture import (  # noqa: E402
    METADATA_SOURCE,
    Capture,
    CaptureSummary,
    RawCapture,
)

__all__ = ["METADATA_SOURCE", "Capture", "CaptureSummary", "RawCapture"]
