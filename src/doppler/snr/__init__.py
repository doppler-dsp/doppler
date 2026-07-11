# snr/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .snr import snr_data_aided_db, snr_m2m4_db, snr_data_aided_db_series, snr_m2m4_db_series  # noqa: E402

__all__ = ["snr_data_aided_db", "snr_m2m4_db", "snr_data_aided_db_series", "snr_m2m4_db_series"]
