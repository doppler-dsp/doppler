# FFT

Each `FFT` object owns one plan and is reused across calls.  Multiple
instances of different sizes or signs coexist with no global state.

## 1-D FFT

```python
from doppler.spectral import FFT
import numpy as np

rng = np.random.default_rng(0)
x32 = (rng.standard_normal(1024) + 1j * rng.standard_normal(1024)).astype(np.complex64)

f = FFT(1024)
X32 = f.execute_cf32(x32)    # complex64 in → complex64 out
print(X32.dtype)

# Parseval: Σ|x|² == Σ|X|² / N
err = abs(np.sum(np.abs(x32)**2) - np.sum(np.abs(X32)**2) / 1024)
print(f"Parseval error: {err:.3e}")
```

```text
complex64
Parseval error: 2.441e-04
```

CF64 input auto-selects the double-precision path:

```python
x64 = rng.standard_normal(1024) + 1j * rng.standard_normal(1024)
X64 = f.execute_cf64(x64)    # complex128 in → complex128 out
print(X64.dtype)
```

```text
complex128
```

## Reusing a plan

Plan once, transform many times — no reallocation on repeated calls:

```python
f = FFT(1024)

for _ in range(1000):
    out = f.execute_cf32(x32)              # out-of-place, returns new array
    out = f.execute_inplace_cf32(x32)     # uses internal buffer, same result
```

## 2-D FFT

```python
from doppler.spectral import FFT2D
import numpy as np

rng = np.random.default_rng(0)
x = (rng.standard_normal((64, 64)) + 1j * rng.standard_normal((64, 64))).astype(np.complex64)

f2 = FFT2D(64, 64)
out = f2.execute_cf32(x.ravel())   # flat row-major input and output
print(len(out), out.dtype)
```

```text
4096 complex64
```
