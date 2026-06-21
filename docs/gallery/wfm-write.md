# Waveform Write — Compose, Write, Read Back

![waveform write demo](../assets/wfm_write_demo.png)

The shortest path from a `Composer` to a file: build a burst, hand it to
`Writer`, recover it with `Reader`, and compare the envelopes.

## What you're seeing

Time-domain magnitude of a three-phase burst written to a BLUE type-1000
file and read back:

- **Tone preamble** — 256 samples, flat envelope, 100 kHz offset, 30 dB SNR.
- **BPSK payload** — 512 symbols at 8 samples/symbol, 10 dB SNR. The
    envelope fluctuates with the AWGN noise floor.
- **Silence** — 256 zero-padded samples; the envelope drops to the noise floor.

The two traces land exactly on top of each other — `cf32` round-trips through
BLUE without loss. BLUE's 512-byte header stores the sample type, byte order,
`fs`, and `fc`, so `Reader` needs no side-channel hints.

## Building it

```python
from doppler.wfm import Composer, Reader, Segment, Writer

segments = [
    Segment(type="tone", freq=100e3, sps=1, fs=1e6, snr=30.0,
            num_samples=256),
    Segment(type="bpsk", sps=8, fs=1e6, snr=10.0,
            num_samples=512 * 8, off_samples=256),
]

burst = Composer(segments).compose()

with Writer("burst.blue", file_type="blue", fs=1e6, fc=915e6) as w:
    w.write(burst)

with Reader("burst.blue") as r:
    readback = r.read(len(burst))
```

## Reproduce

```sh
python examples/python/wfm_write_demo.py   # → burst.blue + wfm_write_demo.png
```

For four containers side by side — raw, CSV, BLUE, and SigMF — see
[Waveform I/O](wfm-io.md).
