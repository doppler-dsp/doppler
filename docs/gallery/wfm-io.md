# Waveform I/O — One Capture, Four Containers

![waveform I/O demo](../assets/wfm_io_demo.png)

A waveform has to land somewhere. `doppler.wfm` writes the same `complex64`
samples to four containers and reads each back — the same C codec behind the
`wfmgen` CLI's `--file-type`. The trade is metadata for size and simplicity.

## What you're seeing

The same QPSK capture written to **raw**, **CSV**, **BLUE type-1000**, and
**SigMF**, then read back; each panel overlays the round-tripped spectrum on the
original (they coincide — the codec is lossless for `cf32`) and annotates the
on-disk size and the metadata the container recovered:

- **raw** — bare interleaved I/Q. Smallest and fastest, but **no metadata**: the
    reader must be *told* the sample type, and `fs`/`fc` are not stored.
- **CSV** — human-readable `I,Q` text. Self-describing shape, no metadata, ~3× the
    bytes (and a tiny text-rounding error).
- **BLUE (type-1000)** — a 512-byte header recording the sample format, byte
    order, and `fs` (as `xdelta = 1/fs`); the reader recovers them with no hints.
- **SigMF** — a `.sigmf-data` + `.sigmf-meta` JSON pair; the most self-describing,
    recovering **both** `fs` and `fc` (and one annotation per segment).

`Reader` auto-detects the container (BLUE magic / `.sigmf-meta` sidecar / `.csv`
extension / else raw); raw is the one case that needs a `sample_type` hint.

## Building it

```python
from doppler.wfm import Composer, Reader, Segment, Writer

seg = Segment("qpsk", fs=1e6, freq=1.5e5, snr=20, sps=8, num_samples=1 << 14)
comp = Composer([seg])
x = comp.compose()

# BLUE: one self-describing file — fs/fc tagged on write, recovered on read.
with Writer("cap.blue", file_type="blue", sample_type="cf32", fs=1e6, fc=2.4e9) as w:
    w.write(x)
with Reader("cap.blue") as r:                 # container auto-detected
    y, fs = r.read(r.num_samples), r.fs        # fs recovered from the header

# SigMF is a pair: Writer lays down .sigmf-data; Composer.to_sigmf() the sidecar.
with Writer("cap.sigmf-data", file_type="sigmf", sample_type="cf32", fs=1e6, fc=2.4e9) as w:
    w.write(x)
open("cap.sigmf-meta", "w").write(
    comp.to_sigmf(sample_type="cf32", fs=1e6, fc=2.4e9)
)
```

The CLI writes the same containers — `wfmgen --file-type raw|csv|blue|sigmf` —
byte-for-byte identical to the Python `Writer`.

## BLUE detached (`.hdr` + `.det`)

BLUE can split the self-describing header from the samples — a 512-byte
`.hdr` Header Control Block plus a raw `.det` body — for pipelines that stream
the payload separately. `write_blue_header` lays down a standalone detached
header; pair it with a raw `Writer` for the body:

```python
from doppler.wfm import Composer, Segment, Writer, write_blue_header

x = Composer([Segment("qpsk", sps=8, num_samples=1 << 14)]).compose()

# Header carries BLUE magic, byte order, data_size, type-1000 tag, xdelta=1/fs.
write_blue_header("cap.hdr", sample_type="cf32", fs=1e6, fc=2.4e9,
                  total=len(x), detached=True)
with Writer("cap.det", file_type="raw", sample_type="cf32") as w:
    w.write(x)                       # raw interleaved I/Q body
```

The CLI does the same in one shot with `--file-type blue --detached -o cap`
(writes `cap.hdr` + `cap.det`); detached output needs `--output` and a finite
(non-`--continuous`) run.

## Real-time streaming (`SampleClock` + `ZmqSink`)

For a live feed, pace a `Composer.stream` against wall-clock with a
`SampleClock` and publish each block over ZeroMQ with a `ZmqSink`. A
`doppler.stream` `Subscriber` receives the frames with their `fs`/`fc` header:

```python
from doppler.stream import Subscriber
from doppler.wfm import Composer, SampleClock, Segment, ZmqSink, qpsk, tone

scene = Segment.sum(qpsk(sps=8, snr=20, snr_mode="esno"),
                    tone(freq=1.2e5, level=-12), num_samples=80_000)
sink = ZmqSink("ipc:///tmp/feed", sample_type="cf64")   # see note below
sub = Subscriber("ipc:///tmp/feed")
clock = SampleClock(fs=500_000)
for block in Composer([scene]).stream(block=4096):
    sink.send(block, 500_000, 2.4e9)
    clock.pace(len(block))                 # throttle to fs
    samples, hdr = sub.recv(timeout_ms=50)
print(clock.samples, clock.underruns, clock.max_lateness)
```

!!! note "Publish cf64/ci32 to a Python subscriber"

    `doppler.stream`'s receiver currently decodes only `CI32`/`CF64`/`CF128`,
    so `ZmqSink`'s default `cf32` (and `ci16`/`ci8`) frames are not yet
    decodable on the Python side — use `sample_type="cf64"` until the
    [stream dtype gap](../dev/wfm-validation-findings.md) is fixed. A C
    `dp_sub_*` subscriber decodes all six types today.

## Reproduce

```sh
python src/doppler/examples/wfm_io_demo.py          # the four-container figure (.png)
python src/doppler/examples/wfm_realtime_stream.py  # paced ZMQ publish + recv stats
```

See the [Python composer API](../api/python-wfmgen.md#compose-multi-segment-composition-writers-and-a-zmq-sink)
for the full `Writer` / `Reader` / `read_iq` surface.
