# Python Spectrum Analyzer API

`doppler.analyzer` is a single CPython extension exposing **`Specan`** — a
streaming spectrum analyzer driven by the *instrument* parameters an operator
already knows (**center, span, RBW, reference level**) instead of the DSP knobs
(window length, Kaiser beta, zero-pad) underneath them.

It is the C-first home for the natural-parameter → DSP mapping: a `Specan`
composes the [`DDC`](python-ddc.md) tuner/decimator and the
[`PSD`](python-spectral.md#averaging-psd-measurements) averaging-PSD core,
so the whole chain lives in C exactly once and the
[`doppler.specan`](../specan/index.md) application is a thin display/transport
shell over it.

______________________________________________________________________

## Specan

```python
import numpy as np
from doppler.analyzer import Specan
from doppler.spectral import find_peaks_f32

# 200 kHz span, 500 Hz RBW around DC of a 2.048 MHz cf32 stream
sa = Specan(fs=2.048e6, span=200e3, rbw=500.0, center=0.0)
sa.fs_out, sa.n, sa.nfft, sa.display_size   # derived DSP grid
sa.rbw, sa.beta                              # realised RBW + the Kaiser beta

rng = np.random.default_rng(0)                # example cf32 source
iq_stream = [(rng.standard_normal(1 << 16)
              + 1j * rng.standard_normal(1 << 16)).astype(np.complex64)
             for _ in range(4)]
for chunk in iq_stream:                       # any cf32 block size
    db = sa.execute(chunk.astype(np.complex64))
    if db is None:                            # not enough samples for a frame
        continue
    # db is a DC-centred dB display band, length sa.display_size.
    # bin i → center + (i − display_size/2)·fs_out/nfft  Hz
    peaks = find_peaks_f32(db, 5, -60.0)      # peaks compose on the trace

sa.retune(50e3)        # cheap, seamless LO retune (no rebuild)
sa.destroy()
```

The constructor maps the instrument parameters to the DSP grid: the span sets
the decimation rate (`fs_out = span·1.28`), the RBW sets the window length
(coarse) and the Kaiser `beta` (fine, solved so the window ENBW realises the
requested RBW), and the display is cropped to the central ±span/2 band. Changing
the **center** is a cheap [`retune`](python-ddc.md) (the same instance keeps its
filter history); changing the **span** or **RBW** alters the decimation rate and
window length, so build a new `Specan`.

`fs`, `span` and `rbw` are **required** — omitting them raises `TypeError`
rather than constructing an unusable analyzer. The display reads in **dBFS**
against the [`PSD`](python-spectral.md) core's 0-dBFS reference — set it with
`bits` (an ADC depth → `2**(bits-1)`) or `full_scale`, the same single-source
knob the measurement analyzers use. `offset_db` is an additive offset applied on
top (e.g. a dBm calibration the application computes from a reference level).
`navg` averages that many segments per emitted frame
(`navg=1` is a responsive single periodogram, larger `navg` trades update rate
for a smoother, lower-variance floor). Peaks are intentionally *not* computed in
the core — compose `find_peaks_f32` on the returned dB band.

::: doppler.analyzer.Specan

______________________________________________________________________

## See also

- [Power Spectra & Measurements guide](../guide/spectral-psd.md) — the
    `time → PSD → measurements` pipeline and the natural-parameter section.
- [Python: spectral API](python-spectral.md) — the `PSD` averaging core a
    `Specan` composes, plus `find_peaks_f32`.
- [Python: DDC](python-ddc.md) — the tuner/decimator front end.
- [Spectrum Analyzer app](../specan/index.md) — the `doppler.specan` display
    shell built on `Specan`.
