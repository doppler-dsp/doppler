# Python Examples

## LO — complex phasor generator

```python
from doppler.source import LO
import numpy as np

# Free-running quarter-rate tone
lo = LO(0.25)
iq = lo.steps(8)
print(iq)
# [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j  1.+0.j  0.+1.j -1.+0.j  0.-1.j]

# FM control port — per-sample frequency deviation
ctrl = (0.002 * np.sin(2 * np.pi * 0.01 * np.arange(1024))).astype(np.float32)
lo2 = LO(0.1)
iq = lo2.steps_ctrl(ctrl)

# Phase continuity: successive calls resume where the last left off
lo3 = LO(0.25)
a = lo3.steps(4)
b = lo3.steps(4)   # seamlessly continues from sample 4
```

## NCO — raw phase accumulator

```python
from doppler.source import NCO
import numpy as np

nco = NCO(0.25)

# Raw uint32 phase values
ph = nco.steps_u32(16)

# Overflow carry: 1 at each accumulator wrap (every 4 samples for 0.25)
carry = nco.steps_u32_ovf(16)
# carry is 1 at indices 3, 7, 11, 15

# Scaled into [0, nmax) — no division, fixed-point multiply
nco2 = NCO(0.25, nmax=1000)
scaled = nco2.steps_u32_scaled(16)   # values in [0, 1000)
```

---

## FFT

### Per-instance FFT (preferred API)

Each `FFT` object owns one plan. Multiple instances of different sizes
or signs coexist with no global state.

```python
from doppler.spectral import FFT, FFT2D
import numpy as np

# CF32 (~2× faster than CF64)
x32 = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
f = FFT(1024)
X32 = f.execute(x32)          # complex64 in → complex64 out
assert X32.dtype == np.complex64

# CF64
x64 = np.random.randn(1024) + 1j * np.random.randn(1024)
X64 = f.execute(x64)          # complex128 in → complex128 out

# In-place
f.execute_inplace(x32)
```

### Reusing a plan (repeated transforms)

```python
from doppler.spectral import FFT
import numpy as np

x = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
f = FFT(1024, nthreads=4)     # plan once

for _ in range(1000):
    out = f.execute(x)        # out-of-place
    # f.execute_inplace(x)    # or in-place
```

### 2-D FFT

```python
from doppler.spectral import FFT2D
import numpy as np

x = (np.random.randn(64, 64) + 1j * np.random.randn(64, 64)).astype(np.complex64)
f2 = FFT2D(64, 64)
out = f2.execute(x)
```

---

## Ring buffers

High-performance double-mapped ring buffers for producer/consumer pipelines:

```python
from doppler import F32Buffer, F64Buffer, I16Buffer
import numpy as np
import threading

# F64 (double-complex) — minimum 256 samples (page-alignment constraint)
buf = F64Buffer(256)

def producer():
    data = np.ones(256, dtype=np.complex128)
    buf.write(data)   # non-blocking; returns False on overflow

def consumer():
    view = buf.wait(256)   # blocks until 256 samples available
    process(view)          # zero-copy view into buffer memory
    buf.consume(256)       # release for reuse

threading.Thread(target=consumer).start()
threading.Thread(target=producer).start()
```

| Type | NumPy dtype | Min samples | Notes |
| ---- | ----------- | ----------- | ----- |
| `F32Buffer` | `complex64` | 512 | float IQ pairs |
| `F64Buffer` | `complex128` | 256 | double IQ pairs |
| `I16Buffer` | `int16, shape=(n,2)` | 1024 | col 0 = I, col 1 = Q |
