"""Acquisition: the searches that find a signal before anything tracks it. Acquisition is the code-phase x Doppler engine; BurstAcquisition and BurstCapture find a burst of any repeated complex preamble (a PN code, Zadoff-Chu, a chirp or a QPSK sequence), and PersistentBurstCapture keeps one armed across calls. CarrierAcquisition is a coarse frequency/phase search that seeds a carrier tracking loop. bin_to_signed is the Doppler-bin fold convention the searches and their hand-offs share.

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

from .acquire import CarrierAcquisition, Acquisition, BurstAcquisition, BurstCapture, PersistentBurstCapture, bin_to_signed  # noqa: E402

__all__ = ["CarrierAcquisition", "Acquisition", "BurstAcquisition", "BurstCapture", "PersistentBurstCapture", "bin_to_signed"]
