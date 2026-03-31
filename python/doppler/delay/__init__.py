"""doppler.delay — dual-buffer cf64 circular delay line.

The delay line keeps the most-recent *num_taps* complex128 samples in a
contiguous memory window at all times, so a polyphase FIR resampler can
MAC directly against the window pointer with no modulo arithmetic.

Internal layout
---------------
Buffer length = 2 × *capacity*, where *capacity* is the smallest power
of two ≥ *num_taps*.  Every :meth:`~DelayCf64.push` writes the new
sample to ``buf[head]`` **and** ``buf[head + capacity]``.
:meth:`~DelayCf64.ptr` always returns ``buf[head:]``, which is
guaranteed contiguous for at least *num_taps* elements.

Classes
-------
DelayCf64
    Wraps :c:type:`dp_delay_cf64_t`.

Examples
--------
Basic push / read-back:

>>> from doppler.delay import DelayCf64
>>> dl = DelayCf64(4)
>>> dl.push(1+2j)
>>> dl.push(3+4j)
>>> dl.ptr()
array([3.+4.j, 1.+2.j, 0.+0.j, 0.+0.j])

Newest sample is always at index 0:

>>> dl.push(5+6j)
>>> complex(dl.ptr()[0])
(5+6j)

Context-manager form:

>>> with DelayCf64(3) as d:
...     _ = d.push_ptr(7+8j)
...     complex(d.ptr()[0])
(7+8j)

Batch push with write():

>>> import numpy as np
>>> dl = DelayCf64(4)
>>> block = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)
>>> dl.write(block)
>>> dl.ptr()
array([4.+0.j, 3.+0.j, 2.+0.j, 1.+0.j])

MAC against a polyphase coefficient row:

>>> dl = DelayCf64(3)
>>> dl.write(np.array([1+0j, 2+0j, 3+0j], dtype=np.complex128))
>>> h = np.array([1, 0, 0], dtype=np.float32)   # select newest tap
>>> complex(np.dot(dl.ptr(), h))
(3+0j)
"""

from ._delay import DelayCf64

__all__ = ["DelayCf64"]
