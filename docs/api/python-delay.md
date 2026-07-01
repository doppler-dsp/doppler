# Python Delay Line API

Dual-write circular delay line for complex128 samples, backed by
`dp_delay_cf64_t`. Designed for polyphase FIR resamplers that need a
contiguous window of history with no modulo arithmetic.

Source:
[`src/doppler/delay/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/delay/__init__.py)

______________________________________________________________________

## How it works

The buffer holds `2 × capacity` samples, where `capacity` is the
smallest power of two ≥ `num_taps`. Every `push` writes the new
sample at `buf[head]` **and** `buf[head + capacity]`. `ptr()` always
returns a contiguous `num_taps`-window — no wrap-around branch needed
by the FIR kernel.

Newest sample is at index 0; oldest at index `num_taps - 1`.

______________________________________________________________________

## Examples

### Basic push and read-back

```python
from doppler.delay import DelayCf64
import numpy as np

dl = DelayCf64(4)           # 4-tap window

dl.push(1+2j)
dl.push(3+4j)

window = dl.ptr()           # array([3+4j, 1+2j, 0+0j, 0+0j])
print(window[0])            # newest: 3+4j
```

### Polyphase FIR inner loop

```python
from doppler.delay import DelayCf64
import numpy as np

num_taps = 19
dl = DelayCf64(num_taps)

taps = np.ones(num_taps, dtype=np.float32) / num_taps   # example taps

iq_stream = (np.random.randn(64)
             + 1j * np.random.randn(64)).astype(np.complex128)
for sample in iq_stream:
    dl.push(sample)
    window = dl.ptr()                       # contiguous num_taps window
    out = np.dot(window, taps.astype(np.complex128))
```

### Push and read in one call

`push_ptr` pushes a sample and returns the updated window in one
round-trip to C, saving one Python call per sample.

```python
for sample in iq_stream:
    window = dl.push_ptr(sample)
    out = np.dot(window, taps)
```

### Context manager

```python
stream = (np.random.randn(64)
          + 1j * np.random.randn(64)).astype(np.complex128)
with DelayCf64(32) as dl:
    for s in stream:
        dl.push(s)
    # dl released on exit
```

______________________________________________________________________

::: doppler.delay.DelayCf64
