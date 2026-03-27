# Python FFT API

High-performance 1-D and 2-D FFT backed by the native C `dp_fft_*`
functions.

Source:
[`python/doppler/fft/_fft.py`](https://github.com/doppler-dsp/doppler/blob/main/python/doppler/fft/_fft.py)

---

## Setup

### `setup(shape, direction=-1, nthreads=1, flag="estimate", wisdom=None)`

Initialise the global FFT plan. Must be called once before any execute
function.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `shape` | tuple[int, ...] | — | Array dimensions, e.g. `(1024,)` or `(64, 64)` |
| `direction` | int | `-1` | `-1` = forward, `1` = inverse |
| `nthreads` | int | `1` | FFTW thread count (use 1 for benchmarks) |
| `flag` | str | `"estimate"` | FFTW planner: `"estimate"`, `"measure"`, `"patient"` |
| `wisdom` | str \| None | `None` | Path to FFTW wisdom file |

```python
from doppler.fft import setup
setup((1024,))                   # 1-D, forward
setup((64, 64), flag="measure")  # 2-D with measured plan
```

---

## 1-D Execute

### `execute1d(input) -> np.ndarray`

Out-of-place 1-D FFT. Returns a new complex128 array.

```python
import numpy as np
from doppler.fft import setup, execute1d

setup((1024,))
x = np.random.randn(1024) + 1j * np.random.randn(1024)
X = execute1d(x)
```

### `execute1d_inplace(data) -> None`

In-place 1-D FFT. Modifies `data` directly (complex128, C-contiguous).

---

## 2-D Execute

### `execute2d(input) -> np.ndarray`

Out-of-place 2-D FFT. Input must be a 2-D complex128 array matching
the shape passed to `setup`.

### `execute2d_inplace(data) -> None`

In-place 2-D FFT.

---

## Generic Execute

### `execute(input) -> np.ndarray`

Dispatches to `execute1d` or `execute2d` based on array rank.

### `execute_inplace(data) -> None`

In-place dispatch.

---

## One-shot Convenience

### `fft(x) -> np.ndarray`

Sets up and executes a forward FFT in one call. Useful for interactive
use; for hot loops call `setup` once and reuse `execute1d`.

```python
from doppler.fft import fft
import numpy as np

x = np.random.randn(1024) + 1j * np.random.randn(1024)
X = fft(x)
```
