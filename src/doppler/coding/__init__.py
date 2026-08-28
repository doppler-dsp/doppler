"""The general channel codes — the families a standard configures, rather than
any standard's picks. `ConvEncoder` and `Viterbi` are the two directions of a
rate-1/n convolutional code and both take the generator polynomials;
`ReedSolomon` is both directions of an RS code over `GF(2**J)` and takes the
five numbers that define one. A caller names their own code rather than
choosing from a menu.

`ccsds_tm` holds CCSDS 131.0-B's configuration of these, the way any other
caller would — and note that a standard adds things that are NOT properties
of the code, such as the dual-basis symbol representation CCSDS transmits its
Reed-Solomon symbols in. Matching the algebra is not the same as matching the
wire.

Examples
--------
>>> import numpy as np
>>> from doppler.coding import ConvEncoder, ReedSolomon, Viterbi
>>> bits = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)
>>> sym = ConvEncoder([0o171, 0o133], k=7).encode(bits)
>>> sym.size == 2 * bits.size
True
>>> llr = np.where(sym, -8.0, 8.0).astype(np.float32)
>>> bits_out = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)
>>> bits_out.dtype
dtype('uint8')
>>> rs = ReedSolomon(nroots=32)             # RS(255,223), corrects 16
>>> rs.n, rs.k, rs.e
(255, 223, 16)"""

# coding/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .coding import ConvEncoder, Viterbi, ReedSolomon, Interleaver, Deinterleaver  # noqa: E402

__all__ = ["ConvEncoder", "Viterbi", "ReedSolomon", "Interleaver", "Deinterleaver"]
