"""Cold-start carrier acquisition: a coarse frequency/phase search (CarrierAcquisition) that seeds a downstream tracking loop.

Examples
--------
>>> import numpy as np
>>> from doppler.acquire import CarrierAcquisition
>>> ca = CarrierAcquisition(sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
...                          resolution_hz=5.0)
>>> ca.ready()
False"""

# acquire/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .acquire import CarrierAcquisition  # noqa: E402

__all__ = ["CarrierAcquisition"]
