# Python Streaming API

`doppler.stream` moves sample blocks between processes (or hosts) over
ZeroMQ, in three socket patterns that share **one wire format** — a
`dp_header_t` (sample rate, centre frequency, sample type, an auto
`timestamp_ns`, and a per-socket `sequence` counter) followed by the raw
sample bytes. Because the header is shared, a C transmitter and a Python
receiver interoperate freely.

Source:
[`src/doppler/stream/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/stream/__init__.py)

For the C side and a runnable end-to-end walk-through, see the
[Streaming examples](../examples/streaming.md).

______________________________________________________________________

## Socket patterns

| Pattern   | Bind / sender | Connect / receiver | Use                                     |
| --------- | ------------- | ------------------ | --------------------------------------- |
| PUB / SUB | `Publisher`   | `Subscriber`       | fan-out broadcast; subscribers may drop |
| PUSH/PULL | `Push`        | `Pull`             | load-balanced pipeline; no drops        |
| REQ / REP | `Requester`   | `Replier`          | request/response (lock-step)            |

Every socket is a **context manager** and releases the GIL while blocked in
ZMQ, so receive loops thread cleanly. The sender's `sample_type` fixes the
NumPy dtype on both ends:

| Constant | dtype               | layout                       |
| -------- | ------------------- | ---------------------------- |
| `CF64`   | `numpy.complex128`  | complex I/Q                  |
| `CF128`  | `numpy.clongdouble` | extended-precision complex   |
| `CI32`   | `numpy.int32`       | interleaved I/Q, length `2n` |

`recv()` returns `(samples, header)` where `header` is a dict of the decoded
`dp_header_t` fields (`sample_rate`, `center_freq`, `sample_type`,
`timestamp_ns`, `sequence`).

______________________________________________________________________

## PUB / SUB — broadcast

The publisher binds and fans out to every connected subscriber; a slow
subscriber drops frames rather than back-pressuring the sender.

<!-- docs-snippet: skip=blocking two-endpoint ZMQ recv; see stream tests -->

```python
import numpy as np
from doppler.stream import Publisher, Subscriber, CF64

# transmitter (binds)
with Publisher("tcp://*:5555", CF64) as pub:
    iq = np.exp(2j * np.pi * 1e3 * np.arange(1000) / 1e6)   # complex128
    pub.send(iq, sample_rate=1e6, center_freq=2.4e9)

# receiver (connects) — in another process
with Subscriber("tcp://localhost:5555") as sub:
    samples, header = sub.recv(timeout_ms=1000)
    print(header["sample_rate"], header["sequence"], len(samples))
```

______________________________________________________________________

## PUSH / PULL — pipeline

PUSH round-robins across connected PULL workers and never drops, giving a
back-pressured work queue.

<!-- docs-snippet: skip=blocking two-endpoint ZMQ recv; see stream tests -->

```python
import numpy as np
from doppler.stream import Push, Pull, CF64

with Push("ipc:///tmp/pipe.ipc", CF64) as push:
    push.send(np.zeros(4096, dtype=np.complex128), sample_rate=1e6)

with Pull("ipc:///tmp/pipe.ipc") as pull:
    samples, header = pull.recv()       # blocks until a frame arrives
```

______________________________________________________________________

## REQ / REP — request/response

A `Requester` sends a sample block and blocks for the `Replier`'s response;
the two alternate strictly.

<!-- docs-snippet: skip=blocking two-endpoint ZMQ recv; see stream tests -->

```python
import numpy as np
from doppler.stream import Requester, Replier, CF64

# server (binds): receive a request, send a reply
with Replier("tcp://*:5556", CF64) as rep:
    req, header = rep.recv()
    rep.send(req * 2)                   # echo-and-scale, say

# client (connects): send, then receive the reply
with Requester("tcp://localhost:5556", CF64) as req:
    req.send(np.ones(256, dtype=np.complex128), sample_rate=1e6)
    resp, header = req.recv()
```

______________________________________________________________________

## Timestamps

`get_timestamp_ns()` returns the same `CLOCK_REALTIME` nanosecond stamp the
senders embed in each header, so a receiver can compute end-to-end latency
against `header["timestamp_ns"]`.

______________________________________________________________________

## PUB / SUB

::: doppler.stream.Publisher

::: doppler.stream.Subscriber

______________________________________________________________________

## PUSH / PULL

::: doppler.stream.Push

::: doppler.stream.Pull

______________________________________________________________________

## REQ / REP

::: doppler.stream.Requester

::: doppler.stream.Replier

______________________________________________________________________

## Helpers

::: doppler.stream.get_timestamp_ns
