# RateConverter

`RateConverter` is a single-class interface to all of doppler's decimation and
interpolation primitives. It inspects the requested rate ratio at construction
time and builds the cheapest cascade — halfband, CIC, or polyphase Resampler —
without any configuration from the caller.

Source: [`src/doppler/examples/rate_converter_demo.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/rate_converter_demo.py)

______________________________________________________________________

## Stage selection

```python
from doppler.resample import RateConverter

rates = [2.0, 0.5, 0.25, 0.125, 0.1, 1/3]
for rate in rates:
    rc = RateConverter(rate)
    print(f"{rate:.4f}  {' → '.join(rc.stages)}")
```

```text
2.0000  Resampler(2)
0.5000  HalfbandDecimator
0.2500  HalfbandDecimator → HalfbandDecimator
0.1250  CIC(8)
0.1000  CIC(8) → Resampler(0.8)
0.3333  Resampler(0.333333)
```

The halfband and CIC paths are the fastest for power-of-two decimation ratios
because they require far fewer multiplications per output sample than a
general polyphase filter. A non-integer ratio like 0.1 (= 1/10) is handled
by rounding to the nearest power-of-two CIC (R=8) and then correcting the
residual 0.8× with the polyphase Resampler.

______________________________________________________________________

## Frequency preservation

A tone at input normalised frequency fn appears at fn/rate in the output —
the RateConverter scales the frequency axis exactly as expected:

```python
import numpy as np
from doppler.resample import RateConverter

fn_in = 0.04
n_in  = 4096
x = np.exp(2j * np.pi * fn_in * np.arange(n_in)).astype(np.complex64)

for rate in [0.5, 0.25, 0.125, 1/3]:
    rc   = RateConverter(rate)
    y    = rc.execute(x)[len(rc.execute(x)) // 20:]   # drop transient
    peak = np.fft.fftfreq(len(y))[np.argmax(np.abs(np.fft.fft(y)))]
    print(f"rate={rate:.4f}  expected fn_out={fn_in/rate:.4f}  "
          f"measured={peak:.4f}")
```

```text
rate=0.5000  expected fn_out=0.0800  measured=0.0802
rate=0.2500  expected fn_out=0.1600  measured=0.1603
rate=0.1250  expected fn_out=0.3200  measured=0.3203
rate=0.3333  expected fn_out=0.1200  measured=0.1203
```

(The small offset is FFT-bin resolution, not resampler error.)

______________________________________________________________________

## Rate change at runtime

Assign to `.rate` to rebuild the cascade in place. Filter state is reset and
the output buffer is reallocated if the new rate requires a larger buffer:

```python
import numpy as np
from doppler.resample import RateConverter

x = np.random.randn(1024).astype(np.complex64)
rc = RateConverter(0.5)

y1 = rc.execute(x); print(len(y1))   # 512

rc.rate = 0.25
y2 = rc.execute(x); print(len(y2))   # 256

rc.rate = 2.0
y3 = rc.execute(x); print(len(y3))   # 2048
```

______________________________________________________________________

## Streaming — phase-continuous across blocks

`execute()` carries filter state across calls, so a stream split at any block
boundary is **byte-identical** to one large call.

!!! warning "The result is a zero-copy view — copy it to keep it"

    `execute()` returns a zero-copy **view** into the converter's internal
    output buffer. The view stays valid only until you next touch the converter
    — **`.copy()` it first if you need to keep it**. Two things invalidate it:

    - **The next `execute()`** reuses the buffer in place — so holding/
        concatenating results without copying gives you the *latest* block's data
        in every earlier array.
    - **`reset()`, assigning `.rate`, or a block larger than any seen so far**
        *reallocates* the buffer — any previously-returned array then points at
        freed memory (don't read it).

    The common streaming loop (fixed block size, consume each block before the
    next call) needs no copy and never reallocates — this only matters if you
    *retain* results across those operations.

```python
import numpy as np
from doppler.resample import RateConverter

x = np.random.randn(2048).astype(np.complex64)

y_full = RateConverter(0.5).execute(x).copy()

rc = RateConverter(0.5)
y_split = np.concatenate([
    rc.execute(x[:1024]).copy(),   # copy: the next execute() reuses the buffer
    rc.execute(x[1024:]).copy(),
])

assert np.array_equal(y_full, y_split)   # byte-identical ✓
```

______________________________________________________________________

## CIC droop compensation

The `compensate=1` flag appends a passband-droop compensating FIR after any
CIC stage, designed with `ciccompmf(N=4, R=R, M=7)`:

```python
from doppler.resample import RateConverter

rc_plain = RateConverter(0.125)
rc_comp  = RateConverter(0.125, compensate=1)
print(rc_plain.stages)   # ['CIC(8)']
print(rc_comp.stages)    # ['CIC(8)+FIR']
```

The compensating FIR corrects the `|sin(x)/x|⁴` roll-off over the CIC
passband at negligible extra cost (7 taps running at the decimated rate).

______________________________________________________________________

## Functional interface

`rate_convert()` is a one-liner wrapper that creates a `RateConverter` on the
first call and returns it so state can be maintained across calls:

```python
from doppler.resample import rate_convert
import numpy as np

x = np.random.randn(4096).astype(np.complex64)

y1, rc = rate_convert(x, 0.5)          # creates RateConverter(0.5)
y2, rc = rate_convert(x, 0.5, rc=rc)   # reuses it — state preserved
```

______________________________________________________________________

Run the full demo:

```bash
python src/doppler/examples/rate_converter_demo.py
```

See [API reference](../api/python-resample.md#rateconverter-automatic-cascade)
for the complete method and property listing.
