"""Soft-decision Viterbi decoding of convolutional codes. The code itself —
polynomials, encoder, trellis arithmetic — lives in the `conv` component;
this is the decoder built over one, so a caller names the polynomials and
gets a decoder for them.

Soft in, hard out: `decode` takes log-likelihood ratios, one per channel
symbol, and returns information bits. A hard-decision decoder throws away
most of the gain the code exists to provide, which is why the input is LLRs.

Examples
--------
>>> import numpy as np
>>> from doppler.viterbi import Viterbi
>>> v = Viterbi([0o171, 0o133], k=7, depth=35)
>>> bits = v.decode(np.array([2.0, -2.0] * 128, dtype=np.float32))
>>> bits.dtype
dtype('uint8')"""

# viterbi/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .viterbi import Viterbi  # noqa: E402

__all__ = ["Viterbi"]
