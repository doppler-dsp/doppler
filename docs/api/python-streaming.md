# Python Streaming API

`doppler.stream` moves sample blocks between processes (or hosts) over
NATS, in three subject-based patterns that share **one wire format** — a
`dp_header_t` (sample rate, centre frequency, sample type, an auto
`timestamp_ns`, and a per-socket `sequence` counter) followed by the raw
sample bytes. Because the header is shared, a C transmitter and a Python
receiver interoperate freely.

Every pattern needs a `nats-server` reachable at the endpoint's
`host:port` — plain `nats-server` is enough for PUB/SUB and REQ/REP;
PUSH/PULL rides the JetStream work-queue tier, so start it with
`nats-server -js`. Endpoints are `nats://host:port/subject`.

Source:
[`src/doppler/stream/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/stream/__init__.py)

For the C side and a runnable end-to-end walk-through, see the
[Streaming examples](../examples/streaming.md).

______________________________________________________________________

## Socket patterns

| Pattern   | Sender      | Receiver     | NATS tier            | Use                                     |
| --------- | ----------- | ------------ | -------------------- | --------------------------------------- |
| PUB / SUB | `Publisher` | `Subscriber` | Core NATS            | fan-out broadcast; subscribers may drop |
| PUSH/PULL | `Push`      | `Pull`       | JetStream work-queue | load-balanced pipeline; durable, acked  |
| REQ / REP | `Requester` | `Replier`    | NATS request/reply   | request/response (lock-step)            |

Every socket is a **context manager** and releases the GIL while blocked
waiting on NATS, so receive loops thread cleanly. The sender's
`sample_type` fixes the NumPy dtype on both ends:

| Constant | dtype               | layout                                                        |
| -------- | ------------------- | ------------------------------------------------------------- |
| `CF32`   | `numpy.complex64`   | complex I/Q                                                   |
| `CF64`   | `numpy.complex128`  | complex I/Q                                                   |
| `CF128`  | `numpy.clongdouble` | extended-precision complex                                    |
| `CI8`    | `numpy.int8`        | interleaved I/Q, length `2n`                                  |
| `CI16`   | `numpy.int16`       | interleaved I/Q, length `2n`                                  |
| `CI32`   | `numpy.int32`       | interleaved I/Q, length `2n`                                  |
| `TLM16`  | structured rows     | telemetry records, not I/Q ([Telemetry](python-telemetry.md)) |

`recv()` returns `(samples, header)` where `header` is a dict of the decoded
`dp_header_t` fields (`sample_rate`, `center_freq`, `sample_type`,
`timestamp_ns`, `sequence`).

______________________________________________________________________

## PUB / SUB — broadcast

The publisher fans out to every subscriber on the subject; a slow
subscriber drops frames rather than back-pressuring the sender. Core NATS
has no backlog, so a subscriber must already be listening before the
publish — start the `Subscriber` first (or add a brief warm-up sleep).

<!-- docs-snippet: skip=blocking two-endpoint NATS recv; see stream tests -->

```python
import numpy as np
from doppler.stream import Publisher, Subscriber, CF64

# transmitter
with Publisher("nats://127.0.0.1:4222/iq", CF64) as pub:
    iq = np.exp(2j * np.pi * 1e3 * np.arange(1000) / 1e6)   # complex128
    pub.send(iq, sample_rate=1e6, center_freq=2.4e9)

# receiver — in another process, started before the publish above
with Subscriber("nats://127.0.0.1:4222/iq") as sub:
    samples, header = sub.recv(timeout_ms=1000)
    print(header["sample_rate"], header["sequence"], len(samples))
```

______________________________________________________________________

## PUSH / PULL — pipeline

PUSH publishes onto a durable JetStream work-queue subject; JetStream
load-balances each frame to exactly one connected PULL worker, giving a
back-pressured, at-least-once work queue. Call `Pull.ack()` once a frame
has been fully processed. Requires a JetStream-enabled broker
(`nats-server -js`).

<!-- docs-snippet: skip=blocking two-endpoint NATS recv; see stream tests -->

```python
import numpy as np
from doppler.stream import Push, Pull, CF64

with Push("nats://127.0.0.1:4222/work", CF64) as push:
    push.send(np.zeros(4096, dtype=np.complex128), sample_rate=1e6)

with Pull("nats://127.0.0.1:4222/work") as pull:
    samples, header = pull.recv()       # blocks until a frame arrives
    pull.ack(samples)
```

______________________________________________________________________

## REQ / REP — request/response

A `Requester` sends a sample block and blocks for the `Replier`'s response;
the two alternate strictly, over NATS's native request/reply.

<!-- docs-snippet: skip=blocking two-endpoint NATS recv; see stream tests -->

```python
import numpy as np
from doppler.stream import Requester, Replier, CF64

# server: receive a request, send a reply
with Replier("nats://127.0.0.1:4222/ctrl", CF64) as rep:
    req, header = rep.recv()
    rep.send(req * 2)                   # echo-and-scale, say

# client: send, then receive the reply
with Requester("nats://127.0.0.1:4222/ctrl", CF64) as req:
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

## Related pages

<!-- related-pages:start -->

**Gallery** — [Waveform I/O — One Capture, Four Containers](../gallery/wfm-io.md)
**Design** — [Telemetry — zero-cost scalar taps for running pipelines](../design/telemetry.md)

<!-- related-pages:end -->
