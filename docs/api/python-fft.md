# Python FFT API

1-D and 2-D FFT backed by the vendored **pocketfft** (pure C99, libm-only).
Each `FFT` / `FFT2D` instance owns an independent plan — no global state,
thread-safe, multiple sizes coexist freely. CF64 transforms run natively.
CF32 transforms run natively in single precision on PFFFT when the length is
a multiple of 16 with no prime factor above 5; any other length is promoted to
double and returned as `complex64`.

Source:
[`src/doppler/spectral/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/spectral/__init__.py)

______________________________________________________________________

## Dtype paths

The path is chosen by the method you call, not by the input's dtype.
`execute_cf32` refuses `complex128` with `TypeError` rather than silently
narrowing it; `execute_cf64` accepts `complex64` and widens it:

| Method         | Input dtype  | C path                                                           | Speed                                                       |
| -------------- | ------------ | ---------------------------------------------------------------- | ----------------------------------------------------------- |
| `execute_cf32` | `complex64`  | PFFFT in float for 16-aligned, 5-smooth lengths; else via double | faster where PFFFT applies (1.6× at N = 1024), else ≈ equal |
| `execute_cf64` | `complex128` | pocketfft in double                                              | baseline                                                    |

______________________________________________________________________

## Examples

### 1-D FFT

```python
from doppler.spectral import FFT
import numpy as np

f = FFT(1024)

# CF32 — single precision (~2× faster)
x32 = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
X32 = f.execute_cf32(x32)
assert X32.dtype == np.complex64

# CF64 — double precision
x64 = np.random.randn(1024) + 1j * np.random.randn(1024)
X64 = f.execute_cf64(x64)
assert X64.dtype == np.complex128

# In-place
f.execute_inplace_cf32(x32)
```

### Inverse FFT

```python
fwd = FFT(1024, sign=-1)   # forward (default)
inv = FFT(1024, sign=+1)   # inverse

X = fwd.execute_cf32(x32)      # x32 from the block above
x_back = inv.execute_cf32(X)   # round-trip (unnormalised — divide by N)
```

### 2-D FFT

```python
from doppler.spectral import FFT2D
import numpy as np

f2 = FFT2D(64, 64)
x = (np.random.randn(64, 64) + 1j * np.random.randn(64, 64)).astype(np.complex64)
X = f2.execute_cf32(x)
f2.execute_inplace_cf32(x)
```

### Repeated transforms

```python
f = FFT(1024)
iq_stream = [x32, x32]             # a couple of complex64 blocks
for block in iq_stream:            # generator of complex64 arrays
    X = f.execute_cf32(block)      # plan reused; no re-allocation
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

## Related pages

<!-- related-pages:start -->

**Guides** — [DSSS Burst Acquisition](../guide/dsss-acquisition.md)
**Design** — [API taxonomy: the DSP building-block hierarchy and its naming axis](../design/api-taxonomy.md), [DSSS acquisition: stateless, parallel, dynamics-capable](../design/dsss-acquisition.md), [Spectral & Measurement API Map](../design/spectral-api-map.md)
**Contributing** — [DSSS Primary Use Cases for Code Acquisition Design](../dev/contributing/dsss-use-cases.md)

<!-- related-pages:end -->
