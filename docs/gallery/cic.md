# CIC Decimation Filter

![CIC decimation spectrum](../assets/cic_demo_spectrum.png)

## What you're seeing

**Top panel — wideband input (0–1024 kHz).**  Two tones are present:
a wanted signal at 15 kHz and a jammer (real cosine) at 208 kHz.
The orange dashed curve is the CIC magnitude response
`|H(f)|` — it rolls off steeply across the input band.  The white
dotted line marks the output Nyquist boundary at 64 kHz; anything
above it will fold back into the passband.

**Bottom panel — decimated output (0–64 kHz).**  After R=16
decimation the wanted tone at 15 kHz dominates the spectrum.  The
jammer at 208 kHz lands in the first alias zone `[128, 256)` kHz
and folds to 48 kHz in the output.  The CIC response at 208 kHz is
≈ −59 dB, so the alias appears ~71 dB below the wanted tone.

## How it works

The CIC transfer function is:

```
|H(f)| = |sin(π f M R / fs) / (R · sin(π f M / fs))|^N
```

N integrator/comb pairs give N-th order roll-off; no multiplications
anywhere.  The SDR pipeline section of the demo builds the composite
input, decimates once, then measures the RMS difference:

```python
from doppler.filter import CIC
import numpy as np

fs_in  = 2.048e6
R, N   = 16, 4
fs_out = fs_in / R          # 128 ksps

fn_wanted = 15e3  / fs_in   # normalised frequency
fn_jammer = 208e3 / fs_in

N_IN = 8 * R * 48
# Real cosine jammer: both ±208 kHz components alias to ±48 kHz
jammer = np.cos(
    2 * np.pi * fn_jammer * np.arange(N_IN)
).astype(np.complex64)
x = (
    np.exp(2j * np.pi * fn_wanted * np.arange(N_IN)).astype(np.complex64)
    + 0.5 * jammer
)

cic = CIC(R, N, 1)          # R=16 decimation, N=4 stages, M=1
y   = np.array(cic.decimate(x), copy=True)
```

Transient settling takes approximately `N*(R-1)/R` output samples;
drop those before measuring power.

```bash
python examples/python/cic_demo.py   # → cic_demo_spectrum.png
```

See [`doppler.filter.CIC`](../api/python-filter.md) for the full API
reference.
