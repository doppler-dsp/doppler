# filter/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

from .filter import FIR, CIC  # noqa: E402
from .cic_design import (  # noqa: E402
    cic_precision_bits,
    cic_alias_rejection,
    cic_passband_droop,
    cic_min_order,
    cic_design,
)

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

__all__ = [
    "FIR",
    "CIC",
    "cic_precision_bits",
    "cic_alias_rejection",
    "cic_passband_droop",
    "cic_min_order",
    "cic_design",
]
