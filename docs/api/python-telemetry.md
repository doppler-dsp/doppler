# Python Telemetry API

Scalar telemetry taps for running DSP objects, backed by `dp_tlm`
(`native/inc/dp_tlm/dp_tlm_core.h`): a named probe registry plus a
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

| Member                    | Purpose                                                                                     |
| ------------------------- | ------------------------------------------------------------------------------------------- |
| `probe(name, decim=1)`    | Register (idempotent by name); emit every `decim`-th event                                  |
| `probe_id(name)`          | Name → id (`KeyError` if unknown)                                                           |
| `probe_names`             | Full `name -> id` map (a **property**, not a call)                                          |
| `emit(id, v)`             | Producer-side record (Python events / tests)                                                |
| `set_now(n)`              | Stamp the sample index carried by subsequent records                                        |
| `read(n=0)`               | Drain into a structured array — `0` means everything available; non-blocking, consumer side |
| `emitted(id)`             | Records written for one probe (post-decimation, post-drop)                                  |
| `dropped`                 | Ring-overrun count (monotonic)                                                              |
| `capacity`, `probe_count` | Introspection                                                                               |
| `_capsule`                | The `dp_tlm_t *` attach point for instrumented objects                                      |

`read()` returns a NumPy structured array with dtype
`[("n", "<u8"), ("value", "<f4"), ("probe", "<u2"), ("flags", "<u2")]` —
16 bytes per row, the exact C record layout.

## `Capture` and `MemoryCapture` — losslessness you can prove

`read()` drains whatever is *currently* in the ring, and the ring drops on
overrun so the DSP thread can never stall. That is right for the emit path
and useless as an answer to "did I get everything?". A capture answers it,
and does so by arithmetic rather than by a ring size you were asked to
guess:

> No probe emits more than once per input sample, so a block of `N` inputs
> emits at most `probe_count * N` records. Size the ring to that bound and
> drain it to empty at every block boundary, and it **cannot** overflow.

`set_now()` already sits at the top of your block loop, so an existing
`set_now / steps / read` loop becomes lossless by opening a capture and
changing nothing else.

| Constructor                                | Where the records go                               |
| ------------------------------------------ | -------------------------------------------------- |
| `MemoryCapture(tlm, block_samples, clock)` | Accumulated in memory; read back with `.records()` |
| `Capture(tlm, block_samples, path, clock)` | Straight to `path`; the file **is** the capture    |

| Member         | Purpose                                                                 |
| -------------- | ----------------------------------------------------------------------- |
| `records(n=0)` | The captured records as a structured array (`MemoryCapture` only)       |
| `block()`      | Explicit boundary; `set_now()` does this for you                        |
| `close()`      | Final drain and verdict — **raises if anything was dropped**            |
| `count`        | Records captured so far                                                 |
| `dropped`      | Records lost by *this* capture (latched at open); non-zero means a hole |

`block_samples` is the largest number of input samples between two
boundaries — the step of your own block loop, not a buffer size to tune.
Over-stating it costs only memory; under-stating it is the one way to lose
a record.

Three things are worth knowing before you use it:

- **Attach every probe first.** The ring is sized from the probe table, so
    opening before anything is registered gives a bound of zero and a
    `ValueError` that says so.
- **A hole raises, on every exit path.** `close()` reports the verdict and
    so does the destructor, which means a `with` block raises rather than
    swallowing a corrupt capture. A capture with a hole is not a smaller
    capture, it is a wrong one.
- **`Capture` has no `records()`.** In file mode the file is the capture;
    an `AttributeError` says that, where an empty array would read as
    "nothing was captured".

A file capture writes raw 16-byte records with no framing, so `np.fromfile`
reads it directly, plus a `<path>-meta` JSON sidecar carrying the probe
table, the counters and the time base. The time base is **borrowed** from
the pipeline's `wfm.SampleClock` rather than restated — two copies of a
time base drift, and the one written into a file is the copy nobody can
correct afterwards.

```python
import numpy as np

from doppler.agc import AGC
from doppler.telemetry import MemoryCapture, Telemetry
from doppler.wfm import SampleClock

BLOCK = 4096
x = (0.05 * np.ones(8 * BLOCK)).astype(np.complex64)

tlm = Telemetry(1 << 12)
agc = AGC()
agc.set_telemetry(tlm, "agc", 1)  # probes first: they set the bound

with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
    for i in range(0, len(x), BLOCK):
        tlm.set_now(i)  # drains the block just finished
        agc.steps(x[i : i + BLOCK])
    cap.close()  # raises if anything was lost
    # Read inside the block: leaving it frees the capture, and touching it
    # afterwards is a RuntimeError rather than a stale answer.
    recs, dropped = cap.records(), cap.dropped

# Every gain update the AGC made, none missing, each with the sample index
# it happened at — which is what "lossless" buys over a polled read().
assert dropped == 0
assert len(recs) > 0
print(f"{len(recs)} records, gain {recs['value'][0]:.1f} -> "
      f"{recs['value'][-1]:.1f} dB")
```

## `EventLog` — the run's events, as SigMF annotations

Telemetry is a time series at thousands of records a second. A run's
**events** are the opposite shape: a handful a minute, each one a fact
about a *span* of the sample stream — an emitter seeded at sample
48 000, tracking from there, lost 40 seconds later. SigMF already has a
vocabulary for exactly that, so an event is an annotation:
`core:sample_start`, `core:sample_count`, and the holder's own fields
under a `doppler:` namespace.

A `.sigmf-meta` is one JSON document, which a run cannot keep rewriting
for hours, so the two jobs are kept apart. During the run each event is
one JSON object on one line of a flat file, flushed as it is written —
tail it live, and a crash costs at most the event being written. At
`finalize()` those lines become the `annotations` array of a proper
sidecar, built by the same emitter every other doppler sidecar goes
through, so `global` and `captures` come out spelled identically.

| Member                                      | Purpose                                                |
| ------------------------------------------- | ------------------------------------------------------ |
| `field(name, value)`                        | Stage a number for the next event, as `doppler:<name>` |
| `field_str(name, value)`                    | Stage a string for the next event                      |
| `append(start, label, sample_count=0, …)`   | Write one event and consume the staged fields          |
| `set_dataset(name)` / `set_telemetry(path)` | Name the sample file and the record file, once per run |
| `finalize(meta_path, …)`                    | Write the `.sigmf-meta` sidecar; the log stays open    |
| `close()`, `count`                          | Final verdict on the writes; events appended so far    |

Three rules decide what reaches the document, and all three are the same
rule: **state what is known, omit what is not.**

- **A span of 0 is an instant**, and `core:sample_count` is left out. A
    written `0` would claim a measured span of nothing.
- **Absolute frequency edges need the channel's centre**, which a BLUE
    header carries and a NATS frame does not. Give `EventLog` an `fc` and
    an event's band becomes `core:freq_lower_edge` / `upper_edge`; leave
    it unknown and the offset and width are still recorded as
    `doppler:freq_hz` / `doppler:bandwidth_hz`, so nothing you knew is
    lost and nothing you did not is invented.
- **A run nobody recorded is `core:metadata_only`**, SigMF's own word for
    it, rather than a missing `core:dataset` key a reader has to
    interpret.

```python
import json
import tempfile
from pathlib import Path

from doppler.telemetry import EventLog

d = Path(tempfile.mkdtemp())

log = EventLog(d / "run.events", fc=2.4e9)
log.set_dataset("capture.sigmf-data")
log.set_telemetry("run.tlm")

log.field("emitter", 3)
log.field("cn0_db_hz", 47.5)
log.field_str("state", "tracking")
log.append(48_000, "seeded", bandwidth_hz=4.0e6)

log.field("emitter", 3)
log.append(4_048_000, "lost", sample_count=1024)

log.finalize(str(d / "run.sigmf-meta"), fs=1.0e7)
log.close()

meta = json.loads((d / "run.sigmf-meta").read_text())
first = meta["annotations"][0]
assert first["core:sample_start"] == 48_000
assert first["doppler:emitter"] == 3
assert first["core:freq_upper_edge"] == 2.4e9 + 2.0e6
assert "core:sample_count" not in first  # an instant, not a span
assert meta["global"]["doppler:telemetry"]["path"] == "run.tlm"
```

One sidecar therefore indexes all three products of a run — the samples,
the events, the telemetry — each in the format that suits its rate. A
finalize does not end the log: a run that lasts hours can emit a sidecar
an hour and keep appending.

## Instrumented objects

Every tracking loop (and the AGC) exposes
`set_telemetry(tlm, prefix, decim=1)`; compositions forward the attach to
their embedded loops under a dotted sub-prefix. `None` detaches
everything the attach armed.

| Object                | Probes under `<prefix>`                                 | Event rate                  |
| --------------------- | ------------------------------------------------------- | --------------------------- |
| `agc.AGC`             | `.gain_db`                                              | per gain update (amortized) |
| `track.Costas`        | `.lock`, `.e`, `.freq`, `.locked`                       | per dumped symbol           |
| `track.Dll`           | `.e`, `.rate`, `.lock`, `.locked`                       | per code epoch              |
| `track.CarrierNda`    | `.lock`, `.e`, `.freq` + `.agc.gain_db`                 | per sample — use `decim`    |
| `track.SymbolSync`    | `.e`, `.freq`, `.rate`, `.lock`, `.locked`              | per recovered symbol        |
| `track.RateSync`      | `.e`, `.ctrl`, `.rate`, `.lock`, `.locked`, `.mu`       | per recovered symbol        |
| `track.MpskReceiver`  | `.lock`, `.tracking` + `.car.*` + `.sync.*` (11 probes) | per recovered symbol        |
| `track.MpskReceiverR` | `.lock`, `.tracking` + `.car.*` + `.sync.*` (11 probes) | per recovered symbol        |
| `dsss.Despreader`     | `.car.*` (Costas) + `.code.*` (DLL)                     | per code period             |

The decision probes pair with their statistic probes by design:
`Dll`'s `.locked` is the verify-counted
[lock-detector](python-detection.md#lock-verification) output next to
the `.lock` CFAR statistic it is judging, and `MpskReceiver`'s
`.tracking` is the two-way handover state next to the `.lock` carrier
metric — plot the pair and you see exactly where the declare/drop rule
fired, without re-deriving thresholds consumer-side.

`RateSync`'s `.mu` (also `MpskReceiver`'s `.sync.mu`) is the odd one out:
every other timing probe is an *error* or a *correction*, while `.mu` is
the timing NCO's own phase — the terminal resampler's accumulator, in
`[0, 1)` output periods, so the polyphase arm the last output read is
`mu * num_phases`. It answers a question the error signals cannot: a
steady `.mu` means the loop has settled on a sampling phase, one that
slews and wraps means a residual *rate* error still unabsorbed (one wrap
is one output period of slip), and hash means the loop is being driven by
something that is not a timing error. Read it alongside `.e` and `.ctrl`
and the three give cause, response, and result.

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

## Over the wire — TLM16 frames

Telemetry crosses processes as `TLM16` frames on the NATS wire layer: a
C pipeline drains its ring with the `dp_tlm_sink_*` helper
(`stream/tlm_sink.h`, in the optional `libdoppler_stream`
component), and a Python producer publishes `read()` output directly.
Either way, `Subscriber.recv()` decodes the frame back into the exact
`read()` dtype:

<!-- docs-snippet: skip=illustrative multi-process fragment (names span two pipelines; not runnable in one namespace) -->

```python
from doppler.stream import Publisher, Subscriber, TLM16

pub = Publisher("nats://127.0.0.1:4222/tlm", TLM16)
sub = Subscriber("nats://127.0.0.1:4222/tlm")

pub.send(tlm.read())  # the structured array, verbatim
recs, hdr = sub.recv(timeout_ms=2000)
assert hdr["sample_type"] == TLM16
assert recs.dtype.names == ("n", "value", "probe", "flags")
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

`Telemetry.stats()` returns a `TelemetryStats` record by value: the counters the ring keeps.

::: doppler.telemetry.TelemetryStats

## Related pages

<!-- related-pages:start -->

**Gallery** — [Gallery](../gallery/index.md), [M-PSK Receiver — Sizing the Carrier Loop, and Riding a Doppler Profile](../gallery/mpsk-doppler-profile.md), [Capturing All Receiver Telemetry](../gallery/mpsk-telemetry-capture.md), [Full-Chain Lock-Up](../gallery/receiver-lock.md), [Telemetry: Many Emitters, One Consumer](../gallery/telemetry-fanin.md)
**Design** — [Automatic Gain Control](../design/agc.md), [AsyncDsssReceiver — the continuous DSSS receiver, from spec to object](../design/async-dsss-receiver.md), [Design](../design/index.md), [Streaming — one envelope, six roles, two planes](../design/streaming.md), [Telemetry — zero-cost scalar taps for running pipelines](../design/telemetry.md)

<!-- related-pages:end -->
