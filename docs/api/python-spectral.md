# Python Correlation & Detection API

The `doppler.spectral` module is a single CPython extension over the C `spectral`
core. Its FFT engines are documented on the [FFT page](python-fft.md); this page
covers the rest of the module — **correlation** (`Corr`, `Corr2D`), **streaming
detection** (`Detector`, `Detector2D`), and the **spectral helper functions**
(windows, magnitude, peak-finding).

For the statistical-detection side (probability of detection, thresholds, dwell
sizing), see the [Detection Statistics page](python-detection.md).

______________________________________________________________________

## Correlation

`Corr` is a 1-D FFT correlator with coherent integrate-and-dump: it pre-computes
`conj(FFT(ref))` once at construction, so each `execute()` costs two FFTs and `n`
complex multiplies. With `dwell == 1` every call dumps; with a larger `dwell`,
the accumulator coherently integrates that many frames before returning a result
(and returns `None` in between).

```python
import numpy as np
from doppler.spectral import Corr

ref = np.exp(2j * np.pi * 0.1 * np.arange(1024)).astype(np.complex64)
corr = Corr(ref, dwell=1)
frame = ref + 0.1 * (np.random.randn(1024) + 1j * np.random.randn(1024))
out = corr.execute(frame.astype(np.complex64))   # ndarray on a dump, else None
lag = int(np.argmax(np.abs(out)))                 # correlation peak position
```

`Corr2D` is the 2-D analogue over an `ny × nx` grid (flat row-major arrays).

::: doppler.spectral.Corr

::: doppler.spectral.Corr2D

______________________________________________________________________

## Streaming detection

`Detector` wraps a correlator with a **double-mapped ring buffer** so you can
push arbitrary-sized chunks. After each integrate-and-dump it compares the
peak-to-noise test statistic against `threshold` and emits a detection result
when it passes (`threshold = 0.0` fires on every dump). The ring capacity is
`next_pow2(max(n, 512))` complex samples.

```python
import numpy as np
from doppler.spectral import Detector

ref = np.exp(2j * np.pi * 0.1 * np.arange(1024)).astype(np.complex64)
det = Detector(ref, dwell=4, threshold=12.0)      # ~12 dB peak-to-noise

for chunk in stream_chunks():                     # any chunk size
    result = det.execute(chunk.astype(np.complex64))
    if result is not None:
        print("detection:", result)               # lag / metric / sample index
```

`Detector2D` is the 2-D streaming detector over a grid.

::: doppler.spectral.Detector

::: doppler.spectral.Detector2D

______________________________________________________________________

## Spectral helpers

Window functions (`hann_window`, `kaiser_window` + its `kaiser_enbw` equivalent
noise bandwidth), magnitude conversion to dB (`magnitude_db_cf32` /
`magnitude_db_cf64`), and peak finding (`find_peaks_f32`) — the building blocks
for a spectrum display.

```python
import numpy as np
from doppler.spectral import (
    FFT, hann_window, magnitude_db_cf32, find_peaks_f32,
)

x = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
w = np.empty(1024, dtype=np.float32)
hann_window(w)                                    # fill in place
spec = np.empty_like(x)
FFT(1024, -1).execute(x * w, spec)
db = magnitude_db_cf32(spec, lin_floor=1e-12, offset_db=0.0)
peaks = find_peaks_f32(db, n_peaks=5, min_db=-60.0)
```

::: doppler.spectral.hann_window

::: doppler.spectral.kaiser_window

::: doppler.spectral.kaiser_enbw

::: doppler.spectral.magnitude_db_cf32

::: doppler.spectral.magnitude_db_cf64

::: doppler.spectral.find_peaks_f32
