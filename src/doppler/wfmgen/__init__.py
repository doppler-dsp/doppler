# wfmgen/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .wfmgen import PN, bpsk_map, qpsk_map, wfm_awgn_amplitude, wfm_ebno_to_snr_db, Synth  # noqa: E402

__all__ = ["PN", "bpsk_map", "qpsk_map", "wfm_awgn_amplitude", "wfm_ebno_to_snr_db", "Synth"]
