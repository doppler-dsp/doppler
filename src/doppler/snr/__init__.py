"""Signal-to-noise estimation: data-aided and moment-based (M2M4) SNR / Es-N0 estimators for a recovered symbol stream.

Examples
--------
>>> import numpy as np
>>> from doppler.snr import snr_m2m4_db
>>> rng = np.random.default_rng(0)
>>> sym = (2 * rng.integers(0, 2, 20000) - 1).astype(np.complex64)
>>> sym += 0.1 * (rng.standard_normal(20000)
...              + 1j * rng.standard_normal(20000)).astype(np.complex64)
>>> bool(15 < snr_m2m4_db(sym) < 25)
True"""

# snr/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .snr import snr_data_aided_db, snr_m2m4_db, snr_data_aided_db_series, snr_m2m4_db_series  # noqa: E402

__all__ = ["snr_data_aided_db", "snr_m2m4_db", "snr_data_aided_db_series", "snr_m2m4_db_series"]
