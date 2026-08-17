"""M-ary PSK mapping: hard and soft, including differential, symbol map/demap for BPSK, QPSK, and 8-PSK constellations.

Examples
--------
>>> import numpy as np
>>> from doppler.mpsk import mpsk_map, mpsk_demap
>>> bits = np.array([0, 1, 1, 0], np.uint8)
>>> np.array_equal(mpsk_demap(mpsk_map(bits, 4), 4), bits)
True"""

# mpsk/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .mpsk import mpsk_map, mpsk_demap, mpsk_diff_map, mpsk_diff_demap, mpsk_bits_per_symbol, mpsk_soft_demap  # noqa: E402

__all__ = ["mpsk_map", "mpsk_demap", "mpsk_diff_map", "mpsk_diff_demap", "mpsk_bits_per_symbol", "mpsk_soft_demap"]
