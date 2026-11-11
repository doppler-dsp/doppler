"""Saturating fixed-point arithmetic: Q15 (1.15, int16) and Q8 (1.7, int8) add, subtract, multiply, dot product and shifts, and the AccQ15/AccQ8 running sums.

A Q15 value v stands for v / 32768 in [-1, +1); Q8 is the same at 2^-7.
Every operation saturates rather than wrapping, as fixed-point hardware
does: 0.5 + 0.5 reads 32767, never a negative. Multiplication is the
fractional product (a*b) >> 15, so 0.5 x 0.5 = 0.25. The dot products and
the accumulators sum in a 64-bit register, so a long sum cannot overflow
partway.

Examples
--------
>>> from doppler.arith import AccQ15
>>> a = AccQ15()
>>> a.step(100); a.step(50)
>>> a.get()
150

>>> import numpy as np
>>> from doppler.arith import add_q15, mul_q15, shl_q15
>>> a = np.array([16384, 16384, -32768], np.int16)   # 0.5, 0.5, -1.0
>>> b = np.array([16384, 32767, 16384], np.int16)    # 0.5, ~1.0, 0.5
>>> add_q15(a, b).tolist()
[32767, 32767, -16384]
>>> mul_q15(a, b).tolist()
[8192, 16384, -16384]
>>> shl_q15(np.array([16384, -32768], np.int16), 1).tolist()
[32767, -32768]

"""

# arith/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .arith import (  # noqa: E402
    AccQ8,
    AccQ15,
    add_q8,
    add_q15,
    dot_q8,
    dot_q15,
    mul_q8,
    mul_q15,
    shl_i64,
    shl_q8,
    shl_q15,
    shr_i64,
    shr_q8,
    shr_q15,
    sub_q8,
    sub_q15,
)

__all__ = [
    "AccQ15",
    "AccQ8",
    "add_q15",
    "sub_q15",
    "mul_q15",
    "dot_q15",
    "shl_q15",
    "shr_q15",
    "add_q8",
    "sub_q8",
    "mul_q8",
    "dot_q8",
    "shl_q8",
    "shr_q8",
    "shl_i64",
    "shr_i64",
]
