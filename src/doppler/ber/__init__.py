"""Bit-error-rate measurement: a BerMeter that aligns a recovered bit stream to a reference and scores errors.

Examples
--------
>>> import numpy as np
>>> from doppler.ber import BerMeter
>>> truth = (np.arange(64) & 1).astype(np.uint8)
>>> m = BerMeter()
>>> _ = m.set_truth(truth)
>>> m.score((1.0 - 2.0 * truth).astype(np.complex64))
0"""

# ber/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .ber import BerMeter, ber_theory_ser, ber_theory_ber, ber_esn0_db_for_ser, ber_evm_scatter_floor_db, ber_settle_syms, ber_lock_symbol, ber_evm_db, ber_settle_from, FrameMeter  # noqa: E402

__all__ = ["BerMeter", "ber_theory_ser", "ber_theory_ber", "ber_esn0_db_for_ser", "ber_evm_scatter_floor_db", "ber_settle_syms", "ber_lock_symbol", "ber_evm_db", "ber_settle_from", "FrameMeter"]
