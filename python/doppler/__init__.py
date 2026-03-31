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

DSP building blocks
-------------------
>>> from doppler.delay import DelayCf64
>>> from doppler.accumulator import AccCf64
>>> from doppler.polyphase import design_bank
"""

# ruff: noqa: F401
from . import accumulator
from . import buffer
from . import delay
from . import fft
from . import nco
from . import polyphase
from . import resample
from . import stream
from . import window

from .accumulator import AccCf64, AccF32
from .delay import DelayCf64
from .nco import Nco
from .buffer import F32Buffer, F64Buffer, I16Buffer
from .stream import (
    Publisher,
    Subscriber,
    Push,
    Pull,
    Requester,
    Replier,
    CI32,
    CF64,
    CF128,
    get_timestamp_ns,
)

__all__ = [
    # Subpackages
    "accumulator",
    "buffer",
    "delay",
    "fft",
    "nco",
    "polyphase",
    "resample",
    "stream",
    "window",
    # DSP building blocks
    "AccF32",
    "AccCf64",
    "DelayCf64",
    "Nco",
    # Circular buffers
    "F32Buffer",
    "F64Buffer",
    "I16Buffer",
    # Sockets
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
