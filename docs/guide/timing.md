# Real-Time Pacing & Timestamping

doppler can emit samples **at their real sample rate** — mimicking a hardware
sample clock — and **timestamp** them on an ideal timeline. Both come from one
small C core, `timing_core` (`dp_sample_clock_t`), exposed as the
`wfmgen --realtime` CLI flag and the Python
[`SampleClock`](../api/python-wfmgen.md#compose-multi-segment-composition-writers-and-a-nats-sink).

This guide explains *why* software pacing is more subtle than `sleep(N/fs)`,
*how* doppler does it drift-free, and *where* it fits — including the role of the
[virtual-memory ring buffer](#the-streaming-fifo-the-mmap-ring-buffer).

______________________________________________________________________

## The problem: `fs` is normally just metadata

A generator produces samples as fast as the CPU allows. The sample rate `fs`
only labels the data — it sets the BLUE `xdelta`, the NATS header's
`sample_rate`, the SigMF `core:sample_rate`. Nothing actually *waits*.

That's correct for writing a file: you want it done now. But when a **consumer
expects real time** — a live spectrum display, a receiver under test, an SDR
playback emulation — flooding it as fast as possible is wrong. You need each
block to leave on the clock: block *k* of *N* samples at time `k·N/fs`.

______________________________________________________________________

## Why `sleep(N/fs)` per block drifts

The obvious loop accumulates error:

<!-- docs-snippet: skip=illustrative anti-pattern (the WRONG example) -->

```python
for blk in blocks:
    sink.send(blk)
    time.sleep(len(blk) / fs)   # WRONG
```

`sleep` *over*sleeps by an OS-scheduler-dependent slice every iteration, and you
never charge for the time `send` itself took. Those errors **compound**: the
effective rate is always a little under `fs`, and the gap grows without bound.
After a million blocks you can be seconds behind.

## The fix: absolute-deadline scheduling

Anchor an epoch once, then derive every deadline from the **cumulative sample
count** against that fixed epoch — never from summed sleeps:

```
deadline(n) = epoch + n / fs        # n = total samples emitted so far
```

Sleep until that absolute instant. An over- or under-sleep on one block is
**corrected on the next**, because the next deadline is computed fresh from `n`,
not added to the last one. This is what `dp_sample_clock_pace` does
(`clock_nanosleep(TIMER_ABSTIME)` on Linux, a portable `nanosleep`-to-remainder
loop elsewhere). The long-run rate is **exactly `fs`**; the only residual error
is per-block jitter bounded by the OS scheduler, and it averages to zero.

!!! note "Average-rate, not sample-accurate"

    Software on a non-realtime OS gives a **drift-free average rate** with
    bounded jitter — never true sample-clock fidelity. A real sample clock is a
    hardware oscillator clocking a DAC out of a FIFO; software's only job there
    is to keep the FIFO fed. So "mimic a sample clock" in software means: emit
    at the correct average rate **and** carry exact timestamps so the consumer
    can reconstruct the intended clock.

______________________________________________________________________

## Pacing — CLI

`wfmgen --realtime` throttles the emit loop to `fs`. Pacing does **not** alter
the samples; a file written with and without `--realtime` is byte-identical.

```sh
# Stream QPSK to a live receiver at the true 1 MS/s, not as fast as possible.
# Requires a nats-server reachable at the endpoint.
wfmgen --type qpsk --fs 1e6 --sps 8 --continuous --realtime \
       --output nats://127.0.0.1:4222/iq
```

If the producer can't keep up — a block takes longer than its `N/fs` period, an
**underrun** — `wfmgen` keeps the absolute timeline and prints a summary to
stderr at exit (`wfmgen: 3 underrun(s) — worst 1.2 ms behind real time`). Use
`--realtime-resync` to re-anchor to "now" on each underrun instead (staying near
real time at the cost of an inserted gap).

## Pacing — Python

The one-liner equivalent of `--realtime` is `comp.stream(block, realtime=fs)`:
passing `realtime=fs` paces the iterator to an `fs`-Hz clock entirely in C (the
generated `stream` owns a `SampleClock` and sleeps to each block's deadline with
the GIL released — the same clock the CLI uses). With `realtime` omitted (the
default `0.0`), `stream` is a pure drain that yields as fast as it can:

<!-- docs-snippet: skip=unbounded realtime NATS stream -->

```python
from doppler.wfm.compose import Composer, StreamSink

comp = Composer(type="qpsk", sps=8, fs=1e6, continuous=True)
with StreamSink("nats://127.0.0.1:4222/iq") as sink:
    for blk in comp.stream(4096, realtime=1e6):   # paced to fs in C
        sink.send(blk, fs=1e6, fc=0.0)
```

When you need the slack value, the timestamp, or a custom loop, drive a
`SampleClock` directly:

<!-- docs-snippet: skip=unbounded realtime pacing loop -->

```python
from doppler.wfm.compose import Composer, SampleClock, StreamSink

comp = Composer(type="qpsk", sps=8, continuous=True)
clk = SampleClock(fs=1e6)
with StreamSink("nats://127.0.0.1:4222/iq") as sink:
    while True:
        blk = comp.execute(4096)
        sink.send(blk, fs=1e6, fc=0.0)
        clk.pace(len(blk))          # sleep to epoch + n/fs (GIL released)
```

`pace()` returns the slack in seconds (negative = it was late). The sleep
happens in C with the **GIL released**, so a paced producer thread doesn't stall
the rest of the interpreter. Underruns are tallied in `clk.underruns` /
`clk.max_lateness`; `SampleClock(fs, resync=True)` re-anchors on underrun.

### Choosing the block size

The block period `N/fs` is your timing resolution. Pick `N` so the period
comfortably exceeds scheduler jitter (~50 µs–2 ms on stock Linux) — **≥ 1 ms is
a good floor**.

| `fs`     | `N`  | period `N/fs` | comment                                                  |
| -------- | ---- | ------------- | -------------------------------------------------------- |
| 1 MS/s   | 4096 | 4.1 ms        | comfortable                                              |
| 100 kS/s | 1024 | 10 ms         | very smooth                                              |
| 50 MS/s  | 4096 | 82 µs         | below jitter — pace per *block*, buffer absorbs the rest |

At high `fs` you can't pace every sample; you pace per block and let the
consumer's buffer absorb the jitter.

______________________________________________________________________

## Timestamping

The same clock answers "when does this block belong?" without a syscall:

```
stamp(n) = epoch_real + n / fs      # ns since the UNIX epoch (CLOCK_REALTIME)
```

```python
import numpy as np
from doppler.wfm.compose import SampleClock

blk = np.zeros(4096, dtype=np.complex64)   # one generated block
clk = SampleClock(fs=1e6)
ts = clk.stamp()        # ideal ns timestamp of the next sample
clk.pace(len(blk))
```

This is a **drift-free idealized timeline**, distinct from the wall-clock instant
a block happens to be transmitted. Use it for reproducible capture metadata:
SigMF `core:datetime`, per-record timestamps, aligning a generated capture to a
reference epoch.

!!! info "The NATS wire header is already timestamped"

    `StreamSink.send` stamps each block's wire header with `CLOCK_REALTIME` at the
    moment of send (jittery, but truthful for "when it left"). `SampleClock`'s
    stamp is the complementary *ideal* timeline — pace for flow control,
    timestamp for the clean schedule.

______________________________________________________________________

## The streaming FIFO: the mmap ring buffer

Pacing and consumption rarely happen in the same loop. The robust pattern is a
**producer/consumer split** across a FIFO: the producer fills it at the paced
real-time rate, the consumer drains it on its own schedule, and the buffer
absorbs the jitter on both sides.

doppler's [`F32Buffer` / `F64Buffer`](../api/python-buffer.md) is built for exactly this — a
lock-free single-producer/single-consumer ring that uses **virtual-memory
mirroring** (the same physical pages mapped at `A` and `A+N`) so a block that
wraps the end is still one contiguous, zero-copy span. No wrap-around branch, no
copy.

<!-- docs-snippet: skip=unbounded producer/consumer threads -->

```python
import threading
from doppler.buffer import F32Buffer
from doppler.wfm.compose import Composer, SampleClock

ring = F32Buffer(capacity=1 << 20)        # 1 Mi-sample ring

def produce():                            # paced to real time
    comp = Composer(type="qpsk", sps=8, continuous=True)
    clk = SampleClock(fs=1e6)
    while True:
        blk = comp.execute(4096)
        while not ring.write(blk):         # backpressure if consumer is slow
            pass
        clk.pace(len(blk))

threading.Thread(target=produce, daemon=True).start()
while True:
    block = ring.wait(8192)               # zero-copy view across the wrap
    process(block)
    ring.consume(8192)
```

The producer is the only thing that knows about real time; the consumer just
keeps up. `ring.dropped` reports overruns if it doesn't. This mirrors how a real
SDR pipeline works — a hardware clock fills a DMA ring, software drains it — with
`SampleClock` standing in for the oscillator.

______________________________________________________________________

## Applications

- **Live receiver / display under test** — feed a spectrum analyzer or demod a
    signal arriving at its true rate, so buffering, AGC, and sync behave as they
    would on real hardware.
- **SDR playback emulation** — replay a recorded or synthesized capture at the
    original `fs` over NATS, standing in for a radio front-end.
- **Reproducible timestamped captures** — stamp segments on an ideal timeline
    for SigMF metadata that's independent of when the file was written.
- **Soak / throughput tests** — run `--continuous --realtime` for hours and
    watch `underruns` / `dropped` to confirm a pipeline sustains real-time rate.

______________________________________________________________________

## See also

- [Waveform Generator guide](wfmgen/streaming.md) — the `--realtime` flag
    in context.
- [Python `compose` API](../api/python-wfmgen.md) — `SampleClock`, `Composer`,
    `StreamSink`.
