"""Running accumulators: a real float32 sum (AccF32), a complex128 sum (AccCf64) and a per-bin trace accumulator (AccTrace), each carrying its total across calls.

`AccF32` and `AccCf64` are integrate-and-dump registers: `step` and
`steps` add, `madd(x, h)` adds a dot product (the inner loop of a
polyphase branch), `get` reads and `dump` reads and clears. `AccTrace`
keeps one running value per bin of a fixed-length frame instead of
reducing it to one number, with `mode` `"mean"`, `"exp"` (EMA, `alpha`),
`"maxhold"` or `"minhold"`; it is the averaging engine behind
`doppler.spectral.PSD`, and `value()` is None until a frame arrives.

Examples
--------
>>> from doppler.accumulator import AccF32
>>> a = AccF32()
>>> a.step(1.5); a.step(2.5)
>>> a.get()
4.0

>>> import numpy as np
>>> from doppler.accumulator import AccTrace
>>> mean = AccTrace(n=4, mode="mean")
>>> mean.accumulate(np.array([1, 3, 5, 7], np.float32))
>>> mean.accumulate(np.array([3, 5, 7, 9], np.float32))
>>> mean.value().tolist()
[2.0, 4.0, 6.0, 8.0]

"""

# accumulator/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .accumulator import AccCf64, AccF32, AccTrace  # noqa: E402

__all__ = ["AccF32", "AccCf64", "AccTrace"]
