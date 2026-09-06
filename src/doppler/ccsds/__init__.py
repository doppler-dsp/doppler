"""CCSDS 131.0-B TM Synchronization and Channel Coding — the literals the standard picked, beside the general layer rather than inside it.

`asm_bits()` is the Attached Sync Marker as bits, which is what a receiver acquires on: pair it with `doppler.detection.SyncFinder` to find where a CADU starts in a bit stream. The standard's *transforms* are not here and are not coming here — describe a CADU with `doppler.wfm.FrameDesc` and the outer code, the randomiser and the inner code run through the general assembler.

Examples
--------
>>> from doppler.ccsds import asm_bits
>>> b = asm_bits()
>>> int("".join(map(str, b.tolist())), 2) == 0x1ACFFC1D
True"""

# ccsds/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .ccsds import asm_bits  # noqa: E402

__all__ = ["asm_bits"]
