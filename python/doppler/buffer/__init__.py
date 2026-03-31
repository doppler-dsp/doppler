"""doppler.buffer — lock-free SPSC ring buffers.

Wraps the :c:type:`dp_buffer_*` family.  All buffers are
single-producer / single-consumer: one thread writes, another reads.
The ``write`` call is non-blocking (drops samples if full); ``wait``
blocks the consumer until enough samples are available, releasing the
GIL so a producer thread can run concurrently.

Classes
-------
F32Buffer
    float32 ring buffer.
F64Buffer
    float64 ring buffer.
I16Buffer
    int16 ring buffer.

Examples
--------
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.capacity()
1024
>>> buf.write(np.ones(512, dtype=np.float32))
True
"""

from ._buffer import F32Buffer, F64Buffer, I16Buffer

__all__ = ["F32Buffer", "F64Buffer", "I16Buffer"]
