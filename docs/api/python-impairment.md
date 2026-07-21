# Python Impairment API

The `doppler.impairment` module holds **propagation impairments** — things that
*transform* a signal on its way from transmitter to receiver. It is deliberately
distinct from `doppler.source`, whose `AWGN`/`LO`/`NCO` *generate* a signal
rather than act on one.

`DopplerChannel` is the first member: clock Doppler, applied to any complex
baseband stream.

Source:
[`src/doppler/impairment/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/impairment/__init__.py)

See the [Doppler Channel gallery page](../gallery/doppler-channel.md) for the
measured carrier offset, code-slip accumulation, and ramp behaviour.

______________________________________________________________________

## How it works

A Doppler shift is not a frequency offset. Relative motion rescales the entire
received time base, so every clock in the signal moves together — carrier, chip
rate, symbol rate, frame rate. Modelling only the carrier is the common
shortcut, and it removes exactly the error a delay-lock loop exists to track.

`DopplerChannel` derives both halves from one parameter:

- **Time-base dilation** — the stream is resampled at output/input ratio
    `1/(1+d)`. Because the resampling acts on the whole stream, a signal carrying
    `Rc` chips/s and `Rs` symbols/s comes out at `Rc(1+d)` and `Rs(1+d)`
    automatically; there is no per-clock adjustment to keep consistent.
- **Carrier offset** — multiplication by `exp(j·2π·fc·excess(t))`, whose
    instantaneous frequency is `fc·d(t)`.

Both read from the same dilation integral `excess(t) = ∫d dt`, so they cannot
drift apart.

### Doppler is specified in ppm

Parts per million of the *nominal time base*, which makes the parameter
carrier-frequency agnostic: one number is simultaneously an offset in Hz and a
rate error in chips/s. At a 2.5 GHz carrier and a 3.069 Mcps code, 20 ppm is
both **+50 kHz** and **+61.4 chips/s**.

`doppler_rate_ppm_s` ramps `d` linearly for a pass-like geometry — 0.2 ppm/s is
500 Hz/s at 2.5 GHz.

!!! warning "`carrier_hz` is DSP input here, not metadata"

    Elsewhere in this codebase (notably `wfmgen --fc`) the carrier frequency is a
    SigMF annotation that never touches a sample. In `DopplerChannel` it is
    load-bearing: Doppler is dimensionless ppm, and `carrier_hz` is the only
    thing that converts it into Hz. Leaving it at `0` still dilates the clocks
    but leaves the carrier stationary — a physically inconsistent capture, and
    permitted only because it is occasionally useful for isolating a code loop
    under test.

### The ramp is an integral

`excess(t) = d0·t + ½·ḋ·t²`. Accumulating `t·d(t)` instead would double-count
the ramp and put the instantaneous offset at `fc·(d0 + 2·ḋ·t)` — exactly twice
the intended Doppler rate. That error passes every static-Doppler check, so both
the C and Python suites assert against it specifically.

______________________________________________________________________

## Usage

```python
import numpy as np

from doppler.impairment import DopplerChannel

FS = 6.138e6  # 3.069 Mcps at spc=2

ch = DopplerChannel(
    fs=FS,
    carrier_hz=2.5e9,
    doppler_ppm=20.0,  # +50 kHz at this carrier
    doppler_rate_ppm_s=0.2,  # 500 Hz/s
)

x = np.ones(65536, dtype=np.complex64)
y = ch.execute(x)
```

Output length is approximately `len(x)/(1+d)` — the missing samples *are* the
dilation:

```python
assert len(y) < len(x)
```

Progress through the stream is readable, and `offset_hz` reflects the ramp:

```python
assert ch.elapsed_s > 0.0
assert ch.offset_hz > 50_000.0  # d0 plus whatever the ramp has added
```

### Streaming

`execute` carries state between calls, so feeding blocks matches one large call.
Feed at most `65536` samples per call (see `DOPPLER_CHANNEL_MAX_BLOCK` in the C
header for why the bound cannot depend on the input length):

```python
ch.reset()
out = [ch.execute(x[i : i + 4096]) for i in range(0, len(x), 4096)]
y2 = np.concatenate(out)
```

### Checkpoint / resume

Like every stateful object here, it resumes bit-for-bit from a blob:

```python
blob = ch.get_state()
other = DopplerChannel(
    fs=FS, carrier_hz=2.5e9, doppler_ppm=20.0, doppler_rate_ppm_s=0.2
)
other.set_state(blob)
```

______________________________________________________________________

::: doppler.impairment.DopplerChannel

## Related pages

<!-- related-pages:start -->

**Gallery** — [Async DSSS Receiver: the SPEC waveform through coupled Doppler](../gallery/async-dsss-receiver-spec.md), [Doppler Channel — Clock Doppler as a Propagation Impairment](../gallery/doppler-channel.md)

<!-- related-pages:end -->
