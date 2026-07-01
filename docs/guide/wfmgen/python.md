# Python API

The engine is also a Python class — ideal for notebooks and pipelines. Every CLI
flag is a keyword argument, and the same defaults apply. For the full generated
reference (every class and method), see the
[Python: Waveform Generator API](../../api/python-wfmgen.md).

```python
import numpy as np
from doppler.wfm import Synth

synth = Synth(type="qpsk", fs=1e6, snr=12.0, snr_mode="esno", sps=8, seed=1)

block = np.asarray(synth.steps(4096))   # → complex64 ndarray
one   = synth.step()                     # → a single complex64 sample
synth.reset()                            # restart the sequence (keeps config)
```

## The builders and the composer

The composition layer mirrors the [concepts](concepts.md) ladder. Builders
`tone()` / `bpsk()` / `qpsk()` / `pn()` / `noise()` / `chirp(f_start=…, f_end=…)`
/ `bits(pattern=…, modulation=…)` each return a `Synth`; `Segment.sum` mixes
them, `.add` / `Timeline` sequences them, and `Composer` renders:

```python
from doppler.wfm import Composer, Segment, Writer, qpsk, tone, read_iq

# Mix a QPSK signal of interest under a CW interferer, one noise floor.
scene = Segment.sum(qpsk(snr=15, sps=8, level=-10), tone(freq=2e5, level=-3),
                    num_samples=65536)
iq = Composer(scene).compose()                 # one complex64 array

# Stream block-by-block into a container (empty block marks the end):
c = Composer(scene)
with Writer("frame.cf32", sample_type="cf32") as w:
    while len(blk := c.execute(4096)):
        w.write(blk)
back = read_iq("frame.cf32", "cf32")           # zero-copy complex64 view
```

Remember the timing gotcha from [concepts](concepts.md#gotcha-where-timing-lives):
`num_samples` / `off_samples` go on `Segment.sum(...)`, not on `Composer(...)`.

## The symbols input — bring your own constellation

`type="symbols"` feeds an arbitrary complex constellation directly — each element
is an output point, oversampled by `sps`, cycled, and RRC-shaped like any
modulation:

```python
import numpy as np
from doppler.wfm import Synth

# pi/4-QPSK: rotate every other QPSK symbol by pi/4, then pass the stream
qpsk = np.array([1 + 1j, -1 + 1j, -1 - 1j, 1 - 1j], np.complex64) / np.sqrt(2)
stream = np.array(
    [qpsk[i % 4] * (np.exp(1j * np.pi / 4) if i % 2 else 1) for i in range(64)],
    np.complex64,
)
iq = Synth(type="symbols", symbols=stream, sps=8, pulse="rrc").steps(64 * 8)
```

## Reading a capture back

The `raw` container is **interleaved** I/Q in the chosen sample type, so a naive
`np.fromfile` gets the layout (and, for integers, the scale) wrong. `read_iq`
does the right thing — a zero-copy complex view for the float types, a SIMD
rescale to ±1.0 for the integer types; the container-aware `Reader` also
auto-detects BLUE/SigMF/CSV/raw and recovers `fs`/`fc`/sample-type:

<!-- docs-snippet: skip=illustrative: reads an I/Q capture file you supply -->

```python
from doppler.wfm import read_iq, Reader

iq = read_iq("capture.iq", sample_type="ci16")   # → complex64, ±1.0
with Reader("capture.blue") as r:                 # container auto-detected
    print(r.file_type, r.fs, r.num_samples)
    x = r.read(r.num_samples)
```

`generate → read_iq` is bit-faithful. See
[Type System → Reading interleaved I/Q](../../types.md#reading-interleaved-iq-in-python).

## Module-level helpers

`doppler.wfm` also exports the primitives the engine composes:

| Symbol                                                             | Use                                                      |
| ------------------------------------------------------------------ | -------------------------------------------------------- |
| `PN(poly, seed, length)`                                           | a raw LFSR / PN sequence object                          |
| `bpsk_map(bits)` / `qpsk_map(syms)`                                | map bits/symbol-indices → cf32 constellation points      |
| `wfm_awgn_amplitude(snr_db, signal_power)`                         | AWGN amplitude for a target SNR over fs                  |
| `wfm_ebno_to_snr_db(ebno_db, bits_per_symbol, samples_per_symbol)` | Eb/No → over-fs SNR                                      |
| `rrc_taps(beta, sps, span)` / `dsss_spread(syms, code, sf)`        | pulse-shaping and spreading primitives                   |
| `mls_poly(n)`                                                      | the auto-selected MLS polynomial for register length `n` |
