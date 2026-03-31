"""doppler.stream — ZMQ-based signal streaming.

Wraps the ``dp_stream`` C extension.  All patterns use a two-frame
ZMQ multipart message: a header frame (``SIGS`` magic + ``dp_header_t``)
and a data frame.  C transmitters and Python subscribers are fully
interoperable.

Patterns
--------
PUB / SUB
    One-to-many broadcast.  :class:`Publisher` → :class:`Subscriber`.
PUSH / PULL
    Pipeline (load-balanced).  :class:`Push` → :class:`Pull`.
REQ / REP
    Request–reply.  :class:`Requester` ↔ :class:`Replier`.

Sample types
------------
:data:`CI32`, :data:`CF64`, :data:`CF128`

Examples
--------
>>> from doppler.stream import Publisher, Subscriber, CF64
>>> import numpy as np
>>>
>>> samples = np.array([1+2j, 3+4j], dtype=np.complex128)
>>> with Publisher("tcp://*:5556", CF64) as pub:
...     pub.send(samples, sample_rate=int(1e6), center_freq=int(2.4e9))
"""

from ._stream import (
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
    "Publisher",
    "Subscriber",
    "Push",
    "Pull",
    "Requester",
    "Replier",
    "CI32",
    "CF64",
    "CF128",
    "get_timestamp_ns",
]
