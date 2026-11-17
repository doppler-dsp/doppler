"""IEEE 802.11 non-HT OFDM builders: the minimal MAC with its FCS, the scrambler, and the PPDU's coding chain, for the layouts the OFDM modulator takes.

Examples
--------
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qam16", code_rate="r34")
>>> w.n_dbps
144
>>> w.n_sym(100)
6

"""

# wlan/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .wlan import Wlan  # noqa: E402

__all__ = ["Wlan"]
