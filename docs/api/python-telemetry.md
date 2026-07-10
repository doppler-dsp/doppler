# Python Telemetry API

Scalar telemetry taps for running DSP objects, backed by `dp_tlm`
(`native/inc/telemetry/telemetry.h`): a named probe registry plus a
lock-free SPSC record ring. Instrumented objects attach a `Telemetry`
context and publish scalars (loop stress, AGC gain, lock metrics) from
their hot loops at *event* rate — one predicted-not-taken branch per event
when detached, one 16-byte ring write when attached, drop-and-count on
overrun so a slow reader can never stall the DSP thread. Design:
[Telemetry](../design/telemetry.md).

Source:
[`src/doppler/telemetry/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/telemetry/__init__.py)

______________________________________________________________________

## `Telemetry`

`Telemetry(ring_records=16384)` — ring capacity in records, a power of
two; a sub-page request is rounded up, so read the real size back from
`.capacity`.

| Member                    | Purpose                                                     |
| ------------------------- | ----------------------------------------------------------- |
| `probe(name, decim=1)`    | Register (idempotent by name); emit every `decim`-th event  |
| `probe_id(name)`          | Name → id (`KeyError` if unknown)                           |
| `probe_names()`           | Full `name -> id` map                                       |
| `emit(probe_id, value)`   | Producer-side record (Python events / tests)                |
| `set_now(n)`              | Stamp the sample index carried by subsequent records        |
| `read(max_records=-1)`    | Drain into a structured array — non-blocking, consumer side |
| `emitted(probe_id)`       | Records written for one probe (post-decimation, post-drop)  |
| `dropped`                 | Ring-overrun count (monotonic)                              |
| `capacity`, `probe_count` | Introspection                                               |
| `_capsule`                | The `dp_tlm_t *` attach point for instrumented objects      |

`read()` returns a NumPy structured array with dtype
`[("n", "<u8"), ("value", "<f4"), ("probe", "<u2"), ("flags", "<u2")]` —
16 bytes per row, the exact C record layout.

## Threading model

The ring is single-producer / single-consumer: everything that emits
(attached objects stepping, `emit`/`set_now`) stays on one producer
thread; `read()`/`dropped` may run on one other thread. Register all
probes before the producer starts.

______________________________________________________________________

## Example — probes, records, decimation

Python-side producers emit named probes through the ring (instrumented C
objects attach with their generated `set_telemetry(tlm, prefix, decim=1)`
face and publish the same way from their hot loops):

```python
from doppler.telemetry import Telemetry

tlm = Telemetry(1 << 12)
snr = tlm.probe("rx.snr_db", decim=1)
for block in range(4):
    tlm.set_now(block * 4096)
    tlm.emit(snr, 12.0 + block)

recs = tlm.read()
assert recs["n"].tolist() == [0, 4096, 8192, 12288]
assert recs["value"].tolist() == [12.0, 13.0, 14.0, 15.0]
```
