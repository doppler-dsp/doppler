"""Channel impairments: a DopplerChannel applying carrier offset, delay, and additive white Gaussian noise to a signal for test and simulation.

Examples
--------
>>> import numpy as np
>>> from doppler.impairment import DopplerChannel
>>> ch = DopplerChannel(fs=1e6, carrier_hz=1e5, doppler_ppm=10.0)
>>> bool(ch.execute(np.ones(64, np.complex64)).size > 0)
True"""

# impairment/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .impairment import DopplerChannel  # noqa: E402

__all__ = ["DopplerChannel"]
