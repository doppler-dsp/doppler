"""
doppler: high-performance signal processing and streaming.

Streaming quick-start
---------------------
>>> from doppler import Publisher, Subscriber, CF64
>>> import numpy as np
>>>
>>> samples = np.array([1+2j, 3+4j], dtype=np.complex128)
>>> with Publisher("tcp://*:5555", CF64) as pub:
...     pub.send(samples, sample_rate=int(1e6), center_freq=int(2.4e9))
...
>>> with Subscriber("tcp://localhost:5555") as sub:
...     data, hdr = sub.recv(timeout_ms=500)

FFT quick-start
---------------
>>> import doppler as dp
>>> dp.fft.setup((1024,))
>>> result = dp.fft.execute1d(samples)
"""

from . import fft
from . import dp_buffer
from . import dp_stream

from .dp_buffer import (
    F32Buffer,
    F64Buffer,
    I16Buffer,
)

from .dp_stream import (
    # Socket classes (C extension - zero-copy, fast!)
    Publisher,
    Subscriber,
    Push,
    Pull,
    Requester,
    Replier,
    # Sample-type constants
    CI32,
    CF64,
    CF128,
    # Utilities
    get_timestamp_ns,
)

__all__ = [
    "fft",
    "dp_buffer",
    "dp_stream",
    # Circular buffers
    "F32Buffer",
    "F64Buffer",
    "I16Buffer",
    # Sockets (C extension)
    "Publisher",
    "Subscriber",
    "Push",
    "Pull",
    "Requester",
    "Replier",
    # Sample types
    "CI32",
    "CF64",
    "CF128",
    # Utils
    "get_timestamp_ns",
]
