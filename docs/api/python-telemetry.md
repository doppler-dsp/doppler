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

## Instrumented objects

Every tracking loop (and the AGC) exposes
`set_telemetry(tlm, prefix, decim=1)`; compositions forward the attach to
their embedded loops under a dotted sub-prefix. `None` detaches
everything the attach armed.

| Object               | Probes under `<prefix>`                               | Event rate                  |
| -------------------- | ----------------------------------------------------- | --------------------------- |
| `agc.AGC`            | `.gain_db`                                            | per gain update (amortized) |
| `track.Costas`       | `.lock`, `.e`, `.freq`                                | per dumped symbol           |
| `track.Dll`          | `.e`, `.rate`, `.lock`                                | per code epoch              |
| `track.CarrierNda`   | `.lock`, `.e`, `.freq` + `.agc.gain_db`               | per sample — use `decim`    |
| `track.SymbolSync`   | `.e`, `.freq`, `.rate`                                | per recovered symbol        |
| `track.MpskReceiver` | `.lock` + `.car.*` (+ `.car.agc.gain_db`) + `.sync.*` | per recovered symbol        |
| `dsss.Despreader`    | `.car.*` (Costas) + `.code.*` (DLL)                   | per code period             |

## Threading model

The ring is single-producer / single-consumer: everything that emits
(attached objects stepping, `emit`/`set_now`) stays on one producer
thread; `read()`/`dropped` may run on one other thread. Register all
probes before the producer starts.

______________________________________________________________________

## Example — watching the AGC gain converge

Instrumented objects expose `set_telemetry(tlm, prefix, decim=1)`; the
AGC registers `"<prefix>.gain_db"` and records the loop-filter integrator
once per gain-update event:

```python
import numpy as np

from doppler.agc import AGC
from doppler.telemetry import Telemetry

tlm = Telemetry(1 << 14)
agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
agc.set_telemetry(tlm, "agc", decim=1)

x = np.full(4096, 0.125 + 0j, dtype=np.complex64)  # quiet input
agc.steps(x)

recs = tlm.read()
gain = recs[recs["probe"] == tlm.probe_id("agc.gain_db")]["value"]
assert len(gain) == 4096 // agc.decim  # one record per control update
assert gain[-1] > gain[0]  # commanded gain rises toward the reference
assert tlm.dropped == 0
```

## Example — probes, records, decimation

Python-side producers emit named probes through the same ring:

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
