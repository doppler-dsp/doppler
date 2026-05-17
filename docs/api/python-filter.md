# Python FIR Filter API

Direct-form FIR filter with AVX-512 acceleration, backed by `dp_fir_*`.
Accepts real or complex taps; input dtype is auto-dispatched to the
correct C hot path.

Source:
[`src/doppler/filter/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/filter/__init__.py)

---

## Tap types

| Tap dtype | C path | Cost/tap/sample | When to use |
|-----------|--------|-----------------|-------------|
| `float32` | real | 1 FMA | `scipy.signal.firwin`, any symmetric LP/HP/BP |
| `complex64` | complex | 2 FMA + permute | Hilbert transformer, frequency-shifted designs |

## Input dtype dispatch

`execute(x)` routes to the right C kernel based on `x.dtype`:

| Input dtype | Interpretation | Output |
|-------------|----------------|--------|
| `complex64` | CF32 IQ | `complex64` |
| `int8` | CI8 interleaved I/Q pairs | `complex64` |
| `int16` | CI16 interleaved I/Q pairs | `complex64` |
| `int32` | CI32 interleaved I/Q pairs | `complex64` |

Integer inputs must have an even number of elements; each `(I, Q)` pair
becomes one complex sample with no scaling.

---

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

### SDR front-end: CI16 raw IQ from ADC

```python
from doppler.filter import FIR
from scipy.signal import firwin
import numpy as np

taps = firwin(31, 0.05).astype(np.float32)
filt = FIR(taps)

# raw_iq: int16 array of interleaved [I0, Q0, I1, Q1, ...]
raw_iq = np.frombuffer(adc_buffer, dtype=np.int16)
y = filt.execute(raw_iq)    # CI16 → CF32 in one call
```

### Stream discontinuity

```python
filt.reset()    # zero delay line; tap coefficients preserved
```

---

::: doppler.filter.FIR
