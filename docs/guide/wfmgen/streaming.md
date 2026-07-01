# Streaming — real-time pacing

By default `wfmgen` emits as fast as the CPU allows — `fs` is only metadata (the
BLUE `xdelta`, the ZMQ header). Add **`--realtime`** to throttle the output to
`fs`, so blocks leave on an `epoch + n/fs` schedule — mimicking a hardware
sample clock feeding the sink. This is what you want when a downstream consumer
expects samples to arrive at the real rate (a live spectrum display, an SDR
playback emulation):

```sh
# Stream QPSK to a live receiver at the true 1 MS/s, not as fast as possible
wfmgen --type qpsk --fs 1e6 --sps 8 --continuous --realtime \
       --output zmq://tcp://*:5555
```

The schedule is **drift-free**: each deadline is recomputed from the cumulative
sample count against a fixed epoch, so sleep jitter never accumulates — the
long-run rate is exactly `fs`. Pacing does **not** alter the samples; a file
written with and without `--realtime` is byte-identical.

If the producer can't keep up (a block takes longer than its `N/fs` period — an
*underrun*), `wfmgen` keeps the absolute timeline and prints a summary to stderr
at exit (`wfmgen: 3 underrun(s) — worst 1.2 ms behind real time`). Use
**`--realtime-resync`** instead to re-anchor the clock to "now" on each underrun,
staying near real time going forward at the cost of an inserted gap.

!!! note "Software pacing is average-rate, not sample-accurate"

    On a non-realtime OS you get a drift-free *average* rate with bounded
    per-block jitter, never true sample-clock fidelity. Keep blocks large enough
    that the period `N/fs` comfortably exceeds scheduler jitter, and let the
    consumer's buffer absorb the rest.

______________________________________________________________________

## The same clock in Python — `SampleClock`

The same C core is exposed as `SampleClock`, which paces and timestamps a stream
against an ideal `fs`-Hz clock — throttle a producer to real time and tag blocks
with their ideal timestamp:

<!-- docs-snippet: skip=illustrative real-time pacing loop over reader IQ -->

```python
from doppler.wfm import Composer, SampleClock, ZmqSink

comp = Composer(type="qpsk", sps=8, continuous=True)
clk = SampleClock(fs=1e6)
with ZmqSink("tcp://0.0.0.0:5555") as sink:
    while True:
        blk = comp.execute(4096)
        ts = clk.stamp()              # ideal ns timestamp of this block
        sink.send(blk, fs=1e6, fc=0.0)
        clk.pace(len(blk))            # sleep to epoch + n/fs (GIL released)
```

The schedule is drift-free (deadlines come from the cumulative sample count, not
summed sleeps); underruns are counted in `clk.underruns` / `clk.max_lateness`,
and `SampleClock(fs, resync=True)` re-anchors to "now" on each underrun.
`SampleClock` and `ZmqSink` are POSIX-only. See the
[Python API](python.md) for the full class surface.
