# Python FIR Filter API

Direct-form FIR filter backed by `fir_state_t`.
Accepts real (`float32`) or complex (`complex64`) taps; input must be `complex64`.

Source:
[`src/doppler/filter/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/filter/__init__.py)

______________________________________________________________________

## Tap types

| Tap dtype   | C path  | Cost/tap/sample | When to use                                    |
| ----------- | ------- | --------------- | ---------------------------------------------- |
| `float32`   | real    | 1 FMA           | `scipy.signal.firwin`, any symmetric LP/HP/BP  |
| `complex64` | complex | 2 FMA + permute | Hilbert transformer, frequency-shifted designs |

______________________________________________________________________

## Examples

### Low-pass filter (real taps)

```python
from doppler.filter import FIR
from scipy.signal import firwin
import numpy as np

taps = firwin(63, cutoff=0.1, window="hamming").astype(np.float32)
filt = FIR(taps)

x = np.random.randn(4096).astype(np.complex64)
y = filt.execute(x)     # complex64 out, length 4096
```

### Reusing across blocks (phase-continuous)

```python
from doppler.filter import FIR
from scipy.signal import firwin
import numpy as np

taps = firwin(63, cutoff=0.2).astype(np.float32)
filt = FIR(taps)

for block in stream:                        # generator of complex64 arrays
    out = filt.execute(block)               # state preserved across calls
```

### Complex taps — Hilbert transformer

```python
from doppler.filter import FIR
import numpy as np

# Simple 4-tap complex example; use scipy for real designs
ctaps = np.array([0+1j, 0+1j, 0+1j, 0+1j], dtype=np.complex64) / 4
filt = FIR(ctaps)
print(filt.is_real)   # False
```

### Stream discontinuity

```python
filt.reset()    # zero delay line; tap coefficients preserved
```

______________________________________________________________________

::: doppler.filter.FIR

______________________________________________________________________

## Moving average (boxcar)

`MovingAverage` is a sliding-window boxcar filter over the last `len` complex
samples — one output per input sample (no rate change). Each step adds the new
sample and subtracts the sample leaving the window, so it is **O(1) per sample**
regardless of window length (a running window sum, not a re-summed convolution).
The output is the window mean times an optional output `gain`, folded into a
single cached `scale = gain/len` so applying the gain is free. The delay ring is
a fixed in-struct array, so the state is pointer-free POD: it embeds by value
into a composing object (a carrier loop's I/Q arm, a smoother ahead of a
detector) and serializes as a whole-struct snapshot.

```python
import numpy as np
from doppler.filter import MovingAverage

ma = MovingAverage(2)                       # 2-sample window, unit gain
ma.steps(np.ones(3, np.complex64)).real     # [0.5, 1.0, 1.0] — ramps in

ma2 = MovingAverage(4, gain=2.0)            # gain folded into the mean
y = ma2.step(1.0 + 0.0j)                    # one sample, returns the gained mean
```

::: doppler.filter.MovingAverage
