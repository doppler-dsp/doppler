# Measurement Suite — ADC characterisation

![measure demo](../assets/measure_demo.png)

## What you're seeing

Four views built entirely from `doppler.measure.ToneMeasure`, the IEEE Std 1241
windowed-tone analyser. Each component's power is integrated over its window
**main lobe**, and the noise sum excludes the leakage bins around DC, the
fundamental and each harmonic — see the
[design guide](../design/measurement-suite.md) for the equations.

**(a) Annotated 12-bit ADC capture.** A ~10 MHz tone is quantised to 12 bits and
analysed. The fundamental reads **0 dBFS**, the broadband **~74 dB quantisation
SNR** sets the floor, and SINAD/SFDR are limited by the 2nd harmonic — the worst
spur (flagged as harmonic). The metric box is the full bag returned by one
`analyze()` call.

**(b) ENOB recovers the ideal N-bit resolution.** Sweeping the ADC from 6 to 16
bits, the measured **ENOB tracks the ideal `ENOB = N`** line — the analyser
recovers `SINAD = 6.02 N + 1.76` to within a fraction of a bit.

**(c) Per-harmonic levels.** The individual harmonic distortion products that
**THD** aggregates (THD is their power sum, shown as the dashed line).

**(d) Dynamic range vs input back-off.** SNR, SINAD and SFDR track the input
level as the tone backs off from full scale, while the **full-scale-corrected
ENOB stays flat** — the converter's intrinsic resolution, independent of drive
level.

## Reproduce

```sh
python src/doppler/examples/measure_demo.py
```

## The measurement object

```python
import numpy as np
from doppler.cvt import ADC
from doppler.measure import ToneMeasure

fs, n = 100e6, 1 << 14
tone = 10.017e6                          # non-coherent so quant. noise spreads
x = 0.999 * np.sin(2 * np.pi * tone * np.arange(n) / fs)
codes = ADC(12, 0.0, 0).steps(x.astype(np.float32)).astype(np.float32)

m = ToneMeasure(window="kaiser", n=n, fs=fs, beta=12.0, full_scale=2.0**11)
r = m.analyze(codes)
print(f"SNR {r.snr:.1f} dB  SINAD {r.sinad:.1f} dB  "
      f"SFDR {r.sfdr_dbc:.1f} dBc  ENOB {r.enob:.2f} bits")
# SNR 73.9 dB  SINAD 61.0 dB  SFDR 62.0 dBc  ENOB 9.84 bits
```

`analyze()` returns a named `ToneMetrics` result (attribute access and tuple
unpacking); `m.rbw`, `m.bin_hz` and `r.lobe_bins` report the analysis grid.

The two-tone IMD/TOI and notched-noise NPR analysers have their own gallery page:
[Measurement Suite — IMD & NPR](measure-imd-npr.md).
