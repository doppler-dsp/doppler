"""Carrier and timing tracking: loop filters, Costas / non-data-aided / MPSK carrier recovery, DLL code tracking, symbol-timing and rate sync, and full MPSK receivers.

Examples
--------
>>> import numpy as np
>>> from doppler.track import LoopFilter
>>> y = LoopFilter(bn=0.05, zeta=0.707).steps(np.ones(50))
>>> bool(y[-1] > y[0])
True"""

# track/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .track import LoopFilter, Costas, Dll, SymbolSync, CarrierMpsk, CarrierNda, MpskReceiver, RateSync, MpskReceiverR, BpskReceiver  # noqa: E402

__all__ = ["LoopFilter", "Costas", "Dll", "SymbolSync", "CarrierMpsk", "CarrierNda", "MpskReceiver", "RateSync", "MpskReceiverR", "BpskReceiver"]
