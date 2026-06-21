# Correlation and Detection

![Corr / Corr2D / Detector demo](../assets/corr_demo.png)

## What you're seeing

**Left — `Corr` coherent integration.** BPSK PN reference, lag=17,
SNR ≈ −6 dB. With a single frame the peak/mean ratio is ~4.0 —
barely distinguishable from noise. After 8 coherent dwells
(`dwell=8`) it rises to ~7.0, pulling the lag-17 peak cleanly above
the noise floor. Coherent integration improves SNR by `10 log₁₀(M)`
dB.

**Centre — `Corr2D` 2-D template match.** An 8×8 complex template
shifted by (row=3, col=5) is recovered in a single FFT2 call. The
surface peak lands exactly on the injected shift.

**Right — `Detector.push()` stream.** Four signal dwells fire above
`threshold=5`; noise-only dwells stay below it. Each dot is one
dwell's test statistic: peak magnitude divided by local noise
estimate.

## How it works

`execute()` accumulates frames and returns output only on the
`dwell`-th call; all other calls return `None`.

```python
from doppler.spectral import Corr
import numpy as np

rng = np.random.default_rng(42)
N, LAG, DWELL = 64, 17, 8
SNR = 10 ** (-6 / 20)   # −6 dB amplitude SNR

ref = rng.choice(
    np.array([-1., 1.], dtype=np.float32), size=N
).astype(np.complex64)

def frame():
    sig  = np.roll(ref, LAG) * np.float32(SNR)
    re   = rng.standard_normal(N).astype(np.float32)
    im   = rng.standard_normal(N).astype(np.float32)
    return (
        sig
        + (re + 1j * im).astype(np.complex64) * np.float32(1 / np.sqrt(2))
    )

with Corr(ref, dwell=DWELL) as c:
    output = None
    for _ in range(DWELL):
        output = c.execute(frame())

peak_lag = int(np.argmax(np.abs(output)))
```

`Detector` wraps this loop and applies a CFAR threshold so you get
`(lag, peak_mag, noise_est, test_stat)` tuples directly from
`det.push(block)` without managing the dwell counter yourself.

```bash
python src/doppler/examples/corr_demo.py   # → corr_demo.png
```

See [Corr / Detector API walkthrough](../examples/python-corr.md)
for `Detector`, `Corr2D`, and `Detector2D` in full detail.
