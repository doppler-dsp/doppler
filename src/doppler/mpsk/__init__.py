"""M-ary PSK mapping: hard and soft, including differential, symbol map/demap for BPSK, QPSK, and 8-PSK constellations.

This is the decision primitive, and its transmit inverse, that the
`doppler.track.CarrierMpsk` carrier loop and the M-PSK receiver compose.
A symbol's `log2(M)` bits pack LSB-first into one `uint8` label, and that
byte is the Gray code, so a slip to an adjacent point flips one bit.
Constellations are unit amplitude: BPSK {+1, -1}, QPSK (+-1 +- j)/sqrt(2)
and 8-PSK exp(j k pi/4). `mpsk_soft_demap` is the same decision as
`mpsk_demap` written as one log-likelihood ratio per bit, the input a
soft-decision decoder needs; the differential pair resolves the M-fold
phase ambiguity a decision-directed carrier loop leaves.

Examples
--------
>>> import numpy as np
>>> from doppler.mpsk import mpsk_map, mpsk_demap
>>> bits = np.array([0, 1, 1, 0], np.uint8)
>>> np.array_equal(mpsk_demap(mpsk_map(bits, 4), 4), bits)
True

"""

# mpsk/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .mpsk import (  # noqa: E402
    mpsk_bits_per_symbol,
    mpsk_demap,
    mpsk_diff_demap,
    mpsk_diff_map,
    mpsk_map,
    mpsk_soft_demap,
)

__all__ = [
    "mpsk_map",
    "mpsk_demap",
    "mpsk_diff_map",
    "mpsk_diff_demap",
    "mpsk_bits_per_symbol",
    "mpsk_soft_demap",
]
