# Python Examples

## NCO

```python
from doppler import Nco
import numpy as np

# Free-running quarter-rate tone
with Nco(0.25) as nco:
    iq = nco.execute_cf32(8)
    print(iq)
    # [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j  1.+0.j  0.+1.j -1.+0.j  0.-1.j]

# Raw phase + overflow carry
with Nco(0.25) as nco:
    ph, carry = nco.execute_u32_ovf(16)
    # carry is 1 at indices 3, 7, 11, 15

# FM control port
ctrl = 0.002 * np.sin(2 * np.pi * 0.01 * np.arange(1024)).astype(np.float32)
with Nco(0.1) as nco:
    iq = nco.execute_cf32_ctrl(ctrl)

# Phase continuity: split calls resume where the last left off
with Nco(0.25) as nco:
    a = nco.execute_cf32(4)
    b = nco.execute_cf32(4)   # seamlessly continues from sample 4
```

---

## FFT

### One-shot FFT

```python
from doppler.fft import fft
import numpy as np

x = np.random.randn(1024) + 1j * np.random.randn(1024)
spectrum = fft(x)
print(f"FFT result: {len(spectrum)} bins")
```

### Reusing a plan (faster for repeated transforms)

```python
from doppler.fft import setup, execute1d, execute1d_inplace
import numpy as np

x = np.random.randn(1024) + 1j * np.random.randn(1024)
setup(x.shape, nthreads=4, planner="measure")  # plan once

for _ in range(1000):
    out = execute1d(x)             # out-of-place, returns new array
    # execute1d_inplace(x)         # or modify x in-place
```

### 2D FFT

```python
from doppler.fft import setup, execute2d
import numpy as np

x = np.random.randn(64, 64) + 1j * np.random.randn(64, 64)
setup(x.shape, nthreads=1, planner="estimate")
out = execute2d(x)
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
