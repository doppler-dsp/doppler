"""doppler.accumulator — general-purpose scalar accumulator.

Maintains a running sum that can be read (:meth:`~AccCf64.get`),
read-and-zeroed (:meth:`~AccCf64.dump`), or reset (:meth:`~AccCf64.reset`).
The 1-D :meth:`~AccCf64.madd` operation is the polyphase resampler hot path:
``acc += sum(x[k] * h[k] for k in range(n))``.

Classes
-------
AccF32
    Float32 accumulator wrapping :c:type:`dp_acc_f32_t`.
AccCf64
    Complex128 accumulator (f32 coefficients) wrapping
    :c:type:`dp_acc_cf64_t`.

Examples
--------
**AccF32** — scalar push and dump:

>>> from doppler.accumulator import AccF32
>>> acc = AccF32()
>>> acc.push(1.0)
>>> acc.push(2.0)
>>> acc.dump()
3.0
>>> acc.dump()   # zeroed by previous dump
0.0

**AccF32** — multiply-accumulate (dot product):

>>> import numpy as np
>>> acc = AccF32()
>>> x = np.array([1, 2, 3, 4], dtype=np.float32)
>>> h = np.array([1, 0, 0, 0], dtype=np.float32)
>>> acc.madd(x, h)
>>> acc.dump()
1.0

**AccCf64** — complex madd (polyphase FIR inner product):

>>> from doppler.accumulator import AccCf64
>>> acc = AccCf64()
>>> x = np.array([3+4j, 1+2j, 0+0j], dtype=np.complex128)
>>> h = np.array([1, 0, 0], dtype=np.float32)
>>> acc.madd(x, h)
>>> acc.dump()
(3+4j)

**AccCf64** — get does not clear; dump does:

>>> acc = AccCf64()
>>> acc.push(1+1j)
>>> acc.get()
(1+1j)
>>> acc.get()   # unchanged
(1+1j)
>>> acc.dump()
(1+1j)
>>> acc.get()   # zeroed
0j

Context-manager form:

>>> with AccCf64() as a:
...     a.push(2+3j)
...     a.get()
(2+3j)
"""

from ..dp_accumulator import AccCf64, AccF32

__all__ = ["AccF32", "AccCf64"]
