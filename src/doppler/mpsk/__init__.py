# mpsk/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .mpsk import mpsk_map, mpsk_demap, mpsk_diff_map, mpsk_diff_demap, mpsk_bits_per_symbol  # noqa: E402

__all__ = ["mpsk_map", "mpsk_demap", "mpsk_diff_map", "mpsk_diff_demap", "mpsk_bits_per_symbol"]
