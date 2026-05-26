# CIC Decimation Filter

![CIC decimation spectrum](../assets/cic_demo_spectrum.png)

## What you're seeing

All three panels share the same x-axis (±1024 kHz = ±fs_in/2) so the
signal positions line up vertically across the cascade.

**Top panel — wideband input.**  Two complex tones:
a wanted signal at 15 kHz and a jammer at 208 kHz.
The orange dashed curve is the CIC magnitude response `|H(f)|`.
The white dotted lines mark the output Nyquist boundaries at ±64 kHz;
anything outside will fold back into the passband.

**Middle panel — UQ16 quantized input.**  The same signal after the
CF32 → offset-binary UQ16 → CF32 roundtrip that the CIC applies
internally.  The quantization noise floor is ~−92 dBFS (Q15 SNR),
indistinguishable from the top panel at this scale.

**Bottom panel — decimated output.**  After R=16 decimation, the wanted
tone at 15 kHz survives near full amplitude.  The jammer at 208 kHz
falls in the alias zone and folds to −48 kHz in the output, attenuated
by ~90 dB by the CIC filter.

## How it works

The CIC transfer function is:

```
|H(f)| = |sin(π f M R / fs) / (R · sin(π f M / fs))|^N
```

N=4 integrator/comb pairs give 4th-order roll-off with no multiplications.
Internally the filter converts each CF32 sample to offset-binary UQ16
(`v_Q15 + 32768 → uint64`) so all accumulation is unsigned — intermediate
overflow wraps and cancels exactly across the N stages.

```python
from doppler.resample import CIC
import numpy as np

fs_in  = 2.048e6
R      = 16
fs_out = fs_in / R          # 128 ksps

fn_wanted = 15e3  / fs_in
fn_jammer = 208e3 / fs_in

N_IN = 8 * R * 48
# Scale amplitudes to stay within Q15 full-scale (peak component ≤ 1.0)
x = (0.6 * np.exp(2j * np.pi * fn_wanted * np.arange(N_IN))
   + 0.3 * np.exp(2j * np.pi * fn_jammer * np.arange(N_IN))
    ).astype(np.complex64)

cic = CIC(R)                # R=16, N=4 (fixed), M=1 (fixed)
y   = np.array(cic.decimate(x), copy=True)
```

Transient settling takes approximately `CIC_N*(R-1)/R` output samples;
drop those before measuring power.

```bash
python examples/python/cic_demo.py   # → cic_demo_spectrum.png
```

See [`doppler.resample.CIC`](../api/python-resample.md#cic--cascaded-integrator-comb-decimator)
for the full API reference.
