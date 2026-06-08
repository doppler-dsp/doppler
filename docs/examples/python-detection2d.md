# 2-D Acquisition Grid (GPS/CDMA Framing)

`detection2d_demo.py` frames `Detector2D` as a GPS/CDMA acquisition
search: the correlation surface is an N_DOPPLER × N_CODE_PHASE grid
where each cell tests one (Doppler bin, code-phase offset) hypothesis.
`Corr2D` evaluates all N = N_DOPPLER × N_CODE_PHASE cells in a single
FFT2 call.

Because `Detector2D` reports the peak over all N cells, the system
false-alarm rate is Pfa_sys = 1 − (1 − pfa_cell)^N. The demo applies
a Bonferroni correction to derive the per-cell gate:

```
pfa_cell = 1 − (1 − Pfa_sys)^(1/N)
theta    = det_threshold(pfa_cell) · sqrt(2/π)
```

The reference template is synthesised as a CAZAC-style signal (IFFT of
a unit-amplitude random-phase spectrum) so that its circular
autocorrelation is exactly zero at all non-zero lags, eliminating
coherent sidelobe contamination of the CFAR noise reference.

![2-D acquisition demo](../assets/detection2d_demo.png)

Three panels:

- **Left** — acquisition surface |R[i,j]| after M coherent dwells.
    White cross = injected (Doppler bin, code-phase); red circle = peak.
- **Centre** — Pd vs dwell M: Marcum Q theory + MC operating point.
- **Right** — ROC at operating SNR and dwell: theory, empirical swept
    threshold, and the MC operating point.

```python
from doppler.detection import det_dwell, det_pd, det_threshold
from doppler.spectral import Corr2D, Detector2D
import math, numpy as np

N_DOPPLER, N_CODE_PHASE = 16, 16
N = N_DOPPLER * N_CODE_PHASE   # 256 cells

SNR_DB, SIGMA, PFA = 3.0, 1.0, 1e-3
snr_amp = 10.0 ** (SNR_DB / 20.0)

# Bonferroni correction: N cells per dwell → tight per-cell Pfa
pfa_cell = 1.0 - (1.0 - PFA) ** (1.0 / N)
eta      = det_threshold(pfa_cell)
theta    = eta * math.sqrt(2.0 / math.pi)

M = det_dwell(snr_amp, 0.90, pfa_cell, max_dwell=64)

# CAZAC reference: exactly zero circular autocorrelation off-peak
rng   = np.random.default_rng(0)
spec  = np.exp(1j * rng.uniform(0, 2*np.pi, (N_DOPPLER, N_CODE_PHASE)))
ref2d = (np.sqrt(N) * np.fft.ifft2(spec)).astype(np.complex64)

# Signal amplitude: snr_amp = A * sqrt(N) / SIGMA
A = snr_amp * SIGMA / math.sqrt(N)

det = Detector2D(ref2d, dwell=M, noise_lo=1, noise_hi=N-1, threshold=0.0)
for *_, stat in det.push(signal_block):
    detected = stat > theta
```

```bash
python examples/python/detection2d_demo.py   # → detection2d_demo.png  (~10 s)
```
