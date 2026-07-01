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


def stream_chunks():                              # a real CF32 source
    for _ in range(4):
        yield (np.random.randn(1024)
               + 1j * np.random.randn(1024)).astype(np.complex64)


for chunk in stream_chunks():                     # any chunk size
    for hit in det.push(chunk.astype(np.complex64)):
        print("detection:", hit)                  # (lag, peak, noise, stat)
```

`Detector2D` is the 2-D streaming detector over a grid.

::: doppler.spectral.Detector

::: doppler.spectral.Detector2D

______________________________________________________________________

## Averaging PSD & measurements

`PSD` is a stateful Welch-method (averaging) power-spectral-density estimator —
the single PSD core the [measurement suite](python-measure.md) also consumes (see
the [Power Spectra & Measurements guide](../guide/spectral-psd.md) for the usage
walk-through). A capture longer than `n` is split into `floor(len/n)` segments;
each is windowed, **zero-padded to `nfft = next_pow2(n·pad)`**, FFT'd, converted
to power, fftshifted to DC-centred order and folded into a running average
([`AccTrace`](python-accumulator.md), `mode` of `"mean"` / `"exp"` / `"maxhold"`
/ `"minhold"`). Feed complex baseband with `accumulate()` or real input with
`accumulate_real()`.

```python
import numpy as np
from doppler.spectral import PSD, find_peaks_f32

cf32_capture = (np.random.randn(8192)
                + 1j * np.random.randn(8192)).astype(np.complex64)
w = PSD(n=1024, fs=1e6, window="kaiser", beta=8.0,
          pad=2, full_scale=1.0, bits=0, mode="mean")   # bits>0 -> 2**(bits-1)
w.accumulate(cf32_capture)                 # or w.accumulate_real(f32_capture)
w.n, w.nfft                                # 1024, 2048 (= next_pow2(1024 * 2))

# display spectra (DC-centred, dBFS w.r.t. full_scale)
psd_db = w.psd_db()                        # averaged power spectrum, dB
psd_dbhz = w.psd_dbhz()                    # PSD, dB/Hz (ENBW / fs normalised)

# raw linear power (cg²-normalised; full_scale NOT applied)
two = w.power_twosided()                   # length nfft, DC-centred
one = w.power_onesided()                   # length nfft//2 + 1, folded to [0, fs/2]

# band / level statistics
per_band = w.band_power(np.array([-2e5, -1e5, 1e5, 2e5]))  # dB per band
total = w.total_band_power(np.array([-2e5, -1e5, 1e5, 2e5]))
obw = w.occupied_bw(0.99)                  # occupied bandwidth, Hz
nf = w.noise_floor()                       # median dB level
snr = w.snr(-1e5, 1e5)                     # peak-in-band minus noise floor, dB
sfdr = w.sfdr(min_db=-120.0)               # spurious-free dynamic range, dB

# spectral peaks compose with the free function on the averaged trace:
peaks = find_peaks_f32(w.psd_db(), n_peaks=5, min_db=-60.0)
```

The `pad` factor interpolates the spectrum (finer bin spacing, not finer
resolution); `full_scale` sets the 0-dBFS reference for the dB getters only — the
linear `power_*` accessors are unaffected. All spectra are DC-centred, matching
`find_peaks_f32`'s bin → frequency convention. The PSD getters return `None`
until the first frame is accumulated.

::: doppler.spectral.PSD

______________________________________________________________________

## Spectral helpers

Window functions (`hann_window`, `kaiser_window` + its `kaiser_enbw` equivalent
noise bandwidth and `kaiser_beta_for_sidelobe` window-design helper, and
`blackman_harris_window` for deep sidelobe rejection ~92 dB),
magnitude conversion to dB (`magnitude_db_cf32` /
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
spec = FFT(1024, -1).execute_cf32(x * w)          # returns the transform
db = magnitude_db_cf32(spec, lin_floor=1e-12, offset_db=0.0)
peaks = find_peaks_f32(db, n_peaks=5, min_db=-60.0)
```

::: doppler.spectral.hann_window

::: doppler.spectral.kaiser_window

::: doppler.spectral.kaiser_enbw

::: doppler.spectral.kaiser_beta_for_sidelobe

::: doppler.spectral.blackman_harris_window

::: doppler.spectral.magnitude_db_cf32

::: doppler.spectral.magnitude_db_cf64

::: doppler.spectral.find_peaks_f32

::: doppler.spectral.obw_from_power

::: doppler.spectral.noise_floor_db
