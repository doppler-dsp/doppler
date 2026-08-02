"""Sample-format conversion: vectorized converters between float32 IQ and fixed-point integer formats (int8/16/32, unsigned Q15), plus a scaling ADC front end.

Examples
--------
>>> import numpy as np
>>> from doppler.cvt import F32ToI16, I16ToF32
>>> x = np.array([0.5, -0.25], np.float32)
>>> I16ToF32().steps(F32ToI16().steps(x)).round(3).tolist()
[0.5, -0.25]"""

# cvt/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .cvt import F32ToI16, I16ToF32, F32ToI16U32, F32ToI16U64, I16U32ToF32, I16U64ToF32, F32ToUQ15, UQ15ToF32, ADC, I32ToF32, I8ToF32  # noqa: E402
from .iq import ADCIQ  # noqa: E402

__all__ = ["F32ToI16", "I16ToF32", "F32ToI16U32", "F32ToI16U64", "I16U32ToF32", "I16U64ToF32", "F32ToUQ15", "UQ15ToF32", "ADC", "I32ToF32", "I8ToF32"]
