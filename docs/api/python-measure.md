# Python Measurement API

`doppler.measure.ToneMeasure` analyses one time-domain capture (real or complex)
into the full single-tone ADC / spectral metric bag — SNR, SINAD, THD, THD+N,
SFDR, ENOB, noise floor and the worst spur — plus the accuracy/resolution
metadata. Each component's power is integrated over its window **main lobe**
(IEEE Std 1241); see the [design guide](../design/measurement-suite.md) for the
equations and conventions.

Source:
[`src/doppler/measure/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/measure/__init__.py)

______________________________________________________________________

## Examples

### Single-tone metrics

```python
import numpy as np
from doppler.measure import ToneMeasure

fs, n = 100e6, 1 << 14
x = np.cos(2 * np.pi * 10.017e6 * np.arange(n) / fs).astype(np.float32)

m = ToneMeasure(n=n, fs=fs)      # auto Kaiser window, sized from bits/DR
r = m.analyze(x)                 # named ToneMetrics result
r.enob, r.sfdr_dbc, r.fund_dbfs  # attribute access
snr, sinad, *_ = r               # ...and tuple unpacking
```

### ENOB of an ADC

```python
from doppler.cvt import ADC

codes = ADC(12, 0.0, 0).steps(x).astype(np.float32)
m = ToneMeasure(n=n, fs=fs, bits=12)   # bits sets the dBFS reference
print(round(m.analyze(codes).enob, 2))   # ≈ 12.0 for an ideal 12-bit ADC
```

All three analyzers take the **`bits`** (ADC depth → `2**(bits-1)`) or
**`full_scale`** dBFS knob and read it back from the shared [`PSD`](python-spectral.md)
core, so `bits=B` is identical to `full_scale=2**(B-1)` — one source of truth.
Each also exposes **`spectrum_dbfs(x)`**: the same averaged-PSD dBFS trace its
metrics use, for a display backdrop (no hand-rolled periodogram needed).

### Complex baseband, accuracy metadata, and the spectrum

```python
iq = np.exp(2j * np.pi * 13e6 * np.arange(n) / fs).astype(np.complex64)
r = m.analyze_complex(iq)        # two-sided analysis

m.rbw, m.bin_hz, r.lobe_bins     # resolution vs interpolation grid
spec = m.spectrum_dbfs(x)        # DC-centred dBFS trace (length nfft) for plots
ts = m.time_stats(x)             # crest_db / papr_db / dc_offset / fs_util_pct
```

!!! tip "Resolution vs bin spacing"

    `m.rbw` (resolution bandwidth) is derived from the un-padded length `n`;
    `m.bin_hz` is the zero-padded interpolation grid. Padding sharpens the
    frequency estimate and the plot, but does **not** improve resolution.

### Two-tone IMD and notched-noise NPR

```python
from doppler.measure import IMDMeasure, NPRMeasure

# Two equal tones -> IMD2/IMD3 and the third-order intercept
t = np.arange(n)
two_tone_capture = (
    0.5 * np.cos(2 * np.pi * 10.0e6 * t / fs)
    + 0.5 * np.cos(2 * np.pi * 11.0e6 * t / fs)
).astype(np.float32)
imd = IMDMeasure(n=n, fs=fs)
r = imd.analyze(two_tone_capture)        # r.imd3_dbc, r.toi_dbfs, ...

# Notched-noise loading -> NPR (band/notch geometry are analyze() args)
noise = np.random.randn(n).astype(np.float32)
active_lo, active_hi = 1.0e6, 40.0e6     # loaded band edges (Hz)
notch_lo, notch_hi = 19.0e6, 21.0e6      # the cleared notch (Hz)
guard_hz = 1.0e6
npr = NPRMeasure(n=n, fs=fs, bits=10)
g = npr.analyze(noise, active_lo, active_hi, notch_lo, notch_hi, guard_hz)
print(g.npr_db)

# both expose the same display-spectrum method as ToneMeasure
imd_spec = imd.spectrum_dbfs(two_tone_capture)   # DC-centred dBFS, length nfft
npr_spec = npr.spectrum_dbfs(noise)
```

### Capture planning

```python
from doppler.measure import (
    dp_coherent_freq,
    measure_min_samples,
    measure_proc_gain,
    measure_rec_nfft,
)

n = measure_min_samples(
    fs, target_rbw=1e3, bits=12, dynamic_range_db=0.0, complex_input=0
)
nfft = measure_rec_nfft(n, pad=2)
pg = measure_proc_gain(nfft)
f0 = dp_coherent_freq(fs, 10e6, n)       # leakage-free coherent test tone
```

______________________________________________________________________

::: doppler.measure.ToneMeasure

______________________________________________________________________

::: doppler.measure.IMDMeasure

______________________________________________________________________

::: doppler.measure.NPRMeasure

______________________________________________________________________

::: doppler.measure.measure_min_samples

::: doppler.measure.measure_rec_nfft

::: doppler.measure.measure_proc_gain

::: doppler.measure.dp_coherent_freq
