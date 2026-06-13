# Python FFT API

1-D and 2-D FFT backed by the vendored **pocketfft** (pure C99, libm-only).
Each `FFT` / `FFT2D` instance owns an independent plan — no global state,
thread-safe, multiple sizes coexist freely. CF64 transforms run natively;
CF32 transforms are computed in double precision and returned as `complex64`.

Source:
[`src/doppler/spectral/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/spectral/__init__.py)

______________________________________________________________________

## Dtype dispatch

Pass any dtype — the right C path is chosen automatically:

| Input dtype  | C path       | Speed      |
| ------------ | ------------ | ---------- |
| `complex64`  | CF32 (fftwf) | ~2× faster |
| `complex128` | CF64 (fftw)  | baseline   |

______________________________________________________________________

## Examples

### 1-D FFT

```python
from doppler.spectral import FFT
import numpy as np

f = FFT(1024)

# CF32 — single precision (~2× faster)
x32 = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
X32 = f.execute(x32)
assert X32.dtype == np.complex64

# CF64 — double precision
x64 = np.random.randn(1024) + 1j * np.random.randn(1024)
X64 = f.execute(x64)
assert X64.dtype == np.complex128

# In-place
f.execute_inplace(x32)
```

### Inverse FFT

```python
fwd = FFT(1024, sign=-1)   # forward (default)
inv = FFT(1024, sign=+1)   # inverse

X = fwd.execute(x)
x_back = inv.execute(X)    # round-trip (unnormalised — divide by N)
```

### 2-D FFT

```python
from doppler.spectral import FFT2D
import numpy as np

f2 = FFT2D(64, 64)
x = (np.random.randn(64, 64) + 1j * np.random.randn(64, 64)).astype(np.complex64)
X = f2.execute(x)
f2.execute_inplace(x)
```

### Repeated transforms

```python
f = FFT(1024)
for block in iq_stream:            # generator of complex64 arrays
    X = f.execute(block)           # plan reused; no re-allocation
    power = np.abs(X) ** 2
```

### Multiple sizes in flight

```python
small = FFT(256)
large = FFT(4096)
# Independent plans — coexist with no conflict
```

______________________________________________________________________

::: doppler.spectral.FFT

______________________________________________________________________

::: doppler.spectral.FFT2D
