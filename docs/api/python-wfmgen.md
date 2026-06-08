# Python Waveform Generator API — Synth / PN

Two classes in the `doppler.wfmgen` module, the same C cores that back the
`wavegen` and `wfmgen` command-line tools:

| Class   | Output                               | Use when                                                                   |
| ------- | ------------------------------------ | -------------------------------------------------------------------------- |
| `Synth` | CF32 — the five-type waveform engine | Generate tone / noise / PN / BPSK / QPSK, with optional LO offset and AWGN |
| `PN`    | uint8 — raw LFSR chips (0/1)         | Spreading / ranging codes, scrambling, test vectors                        |

Source:
[`src/doppler/wfmgen/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfmgen/__init__.py)

For the command-line tools built on these cores, see the
[Waveform Generator guide](../guide/wfmgen.md).

______________________________________________________________________

## `Synth` — the five-type waveform engine

One declarative engine produces every waveform type, selected by the string
`type`. Construction takes keyword arguments mirroring the generator flags;
sensible defaults mean a bare `Synth()` is a clean, unit-power baseband tone.

```python
from doppler.wfmgen import Synth
import numpy as np

# Bare construct → clean baseband tone, unit power, no noise
x = Synth().steps(4096)            # complex64

# Tone at Fs/10 with 20 dB SNR
tone = Synth(type="tone", fs=1e6, freq=100_000, snr=20).steps(4096)

# Complex AWGN (unit power)
noise = Synth(type="noise", seed=7).steps(8192)

# PN / BPSK / QPSK — sps samples per chip/symbol
pn   = Synth(type="pn",   pn_length=7, sps=1).steps(127)
bpsk = Synth(type="bpsk", sps=8, snr=10).steps(8192)
qpsk = Synth(type="qpsk", sps=8, snr=10).steps(8192)

# Scalar (one sample at a time)
s = Synth(type="tone", freq=1000, fs=1e6).step()
```

### Clean vs noisy, baseband vs offset

`snr` is in dB. **`snr >= 100` (the default) is clean** — no AWGN is generated
at all, so a clean waveform pays no noise cost. Lower it to add noise.
**`freq = 0` (the default) is baseband** — the LO is skipped entirely.

```python
clean   = Synth(type="qpsk", sps=8, snr=100).steps(8192)   # no AWGN
noisy   = Synth(type="qpsk", sps=8, snr=12).steps(8192)     # Es/No 12 dB
offset  = Synth(type="pn", pn_length=9, sps=1, freq=2.5e5, fs=1e6).steps(511)
```

`snr_mode` (`"auto"`, `"fs"`, `"ebno"`, `"esno"`) sets how `snr` is
interpreted; `"auto"` uses over-`fs` for tone/noise/PN and Es/No for BPSK/QPSK.

### PN modulation: length, polynomial, realization

```python
# Auto-pick the maximum-length polynomial for the register length (2..64)
Synth(type="pn", pn_length=23, sps=1).steps(8192)

# Explicit 64-bit polynomial
Synth(type="pn", pn_length=40, pn_poly=0x800000001C, sps=1).steps(8192)

# Fibonacci realization (same polynomial/period, different chip ordering)
Synth(type="pn", pn_length=9, sps=1, lfsr="fibonacci").steps(511)
```

### Determinism

```python
s = Synth(type="qpsk", sps=4, seed=11)
a = s.steps(512)
s.reset()
assert np.array_equal(a, s.steps(512))   # same seed → identical stream
```

______________________________________________________________________

::: doppler.wfmgen.Synth

______________________________________________________________________

## `PN` — raw LFSR m-sequence

A right-shift LFSR producing one bit (0/1) per call. With a primitive
polynomial it is a **maximum-length sequence**: period `2**n - 1` with
`2**(n-1)` ones per period. Registers up to **64 bits** are supported, in
either the **Galois** (internal-XOR, default) or **Fibonacci** (external-XOR)
realization — both realize the same polynomial and period.

```python
from doppler.wfmgen import PN
import numpy as np

# Length-7 MLS (primitive polynomial 0x41), one full period
chips = np.asarray(PN(0x41, 1, 7).generate(127))   # uint8, 64 ones / 63 zeros

# Fibonacci realization of the same polynomial
fib = np.asarray(PN(0x41, 1, 7, lfsr="fibonacci").generate(127))

# 64-bit register
big = np.asarray(PN(0x800000001C, 1, 40).generate(50_000))

# Deterministic replay
p = PN(0x41, 1, 7)
a = np.asarray(p.generate(127)).copy()
p.reset()
assert np.array_equal(a, np.asarray(p.generate(127)))
```

The constructor is `PN(poly, seed, length, lfsr="galois")`. `seed` must be
non-zero (the all-zero register is a fixed point). To map chips to ±1 BPSK
symbols, use `Synth(type="pn", ...)` instead, which also handles oversampling,
the LO, and AWGN.

______________________________________________________________________

::: doppler.wfmgen.PN

______________________________________________________________________

## `compose` — multi-segment composition, writers, and a ZMQ sink

`doppler.wfmgen.compose` is the Python face of the C `wfmgen` composer
subsystem — the same engine behind the `wfmgen` CLI. A `Composer` strings
`Segment` specs into one stream (with per-segment on-time and trailing gaps),
optionally looping (`repeat`) or running forever (`continuous`); `Writer`
serialises to the same containers as the CLI (raw / CSV / BLUE type-1000 /
SigMF), and `ZmqSink` publishes over ZeroMQ. The resolved spec round-trips
through JSON, so a capture is fully reproducible. Output is byte-identical to the
`wfmgen`/`wavegen` CLIs for the same parameters.

```python
import numpy as np
from doppler.wfmgen.compose import Composer, Segment, Writer, mls_poly
from doppler.wfmgen.readback import read_iq

# A frame: a PN preamble, a QPSK payload, then a guard gap.
spec = [
    Segment("pn", num_samples=127, pn_length=7),
    Segment("qpsk", sps=8, num_samples=4096, seed=42, off_samples=512),
]
iq = Composer(spec).compose()            # one complex64 array

# Or stream block-by-block (an empty block marks the end):
c = Composer(spec)
with Writer("frame.cf32", sample_type="cf32") as w:
    while len(blk := c.execute(4096)):
        w.write(blk)
back = read_iq("frame.cf32", "cf32")     # zero-copy complex64 view

# Reproducible: the resolved spec serialises to JSON and back.
j = Composer(spec).to_json()
assert np.array_equal(Composer.from_json(j).compose(), iq)

# Utilities
mls_poly(7)                              # 0x41 — the length-7 MLS polynomial
```

`Writer` pairs with [`read_iq`](#read_iq); for SigMF, pair a
`Writer(..., file_type="sigmf")` data file with `sigmf_meta(...)`, and for
detached BLUE use `write_blue_header(...)`. The `ZmqSink` is POSIX-only. DSP
helpers `rrc_taps(beta, sps, span)` and `dsss_spread(syms, code, sf)` expose the
pulse-shaping and spreading primitives.

::: doppler.wfmgen.compose.Composer

::: doppler.wfmgen.compose.Segment

::: doppler.wfmgen.compose.Writer

::: doppler.wfmgen.compose.ZmqSink
