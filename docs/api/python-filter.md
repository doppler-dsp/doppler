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
