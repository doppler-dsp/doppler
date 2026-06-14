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

m = ToneMeasure(window="kaiser", n=n, fs=fs, beta=12.0)
r = m.analyze(x)                 # named ToneMetrics result
r.enob, r.sfdr_dbc, r.fund_dbfs  # attribute access
snr, sinad, *_ = r               # ...and tuple unpacking
```

### ENOB of an ADC

```python
from doppler.cvt import ADC

codes = ADC(12, 0.0, 0).steps(x).astype(np.float32)
m = ToneMeasure(n=n, fs=fs, beta=14.0, full_scale=2.0**11)
print(round(m.analyze(codes).enob, 2))   # ≈ 12.0 for an ideal 12-bit ADC
```

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

______________________________________________________________________

::: doppler.measure.ToneMeasure
