# arith/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .arith import AccQ15, AccQ8, add_q15, sub_q15, mul_q15, dot_q15, shl_q15, shr_q15, add_q8, sub_q8, mul_q8, dot_q8, shl_q8, shr_q8, shl_i64, shr_i64  # noqa: E402

__all__ = ["AccQ15", "AccQ8", "add_q15", "sub_q15", "mul_q15", "dot_q15", "shl_q15", "shr_q15", "add_q8", "sub_q8", "mul_q8", "dot_q8", "shl_q8", "shr_q8", "shl_i64", "shr_i64"]
