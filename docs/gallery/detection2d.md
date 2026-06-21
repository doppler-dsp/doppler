# 2-D Acquisition Grid

![2-D acquisition demo](../assets/detection2d_demo.png)

## What you're seeing

GPS/CDMA-style acquisition over a 16×16 Doppler × code-phase grid.
`Corr2D` evaluates all 256 cells in a single FFT2 call.

**Left — `|R[i,j]|` acquisition surface.** The white cross marks the
injected (Doppler bin, code-phase) offset; the red circle marks the
detected peak. Off-peak cells are suppressed by the CAZAC reference
whose circular autocorrelation is exactly zero at all non-zero lags.

**Centre — Pd vs dwell M.** Marcum Q theory curve with the Monte
Carlo operating point overlaid. The per-cell Pfa is tighter than the
system Pfa by the Bonferroni factor (see below).

**Right — ROC at operating SNR and dwell.** Three traces: Marcum Q
theory, empirical swept-threshold curve, and the MC operating point.
Theory and simulation agree throughout.

## How it works

With N=256 cells, the system Pfa budget must be divided across all
cells. The Bonferroni correction gives a conservative per-cell gate:

```
pfa_cell = 1 − (1 − Pfa_sys)^(1/N)
theta    = det_threshold(pfa_cell) · sqrt(2/π)
```

The reference is a CAZAC signal (IFFT of a unit-amplitude
random-phase spectrum) — its circular autocorrelation is exactly
zero off-peak, so there is no coherent sidelobe contamination of the
CFAR noise estimate:

```python
from doppler.detection import det_dwell, det_threshold
from doppler.spectral import Detector2D
import math, numpy as np

N_DOPPLER, N_CODE_PHASE = 16, 16
N = N_DOPPLER * N_CODE_PHASE    # 256 cells

SNR_DB, SIGMA, PFA_SYS = 3.0, 1.0, 1e-3
snr_amp  = 10.0 ** (SNR_DB / 20.0)

# Bonferroni: tight per-cell gate from system Pfa budget
pfa_cell = 1.0 - (1.0 - PFA_SYS) ** (1.0 / N)
eta      = det_threshold(pfa_cell)
theta    = eta * math.sqrt(2.0 / math.pi)

M = det_dwell(snr_amp, 0.90, pfa_cell, max_dwell=64)

# CAZAC reference: zero circular autocorrelation off-peak
rng  = np.random.default_rng(0)
spec = np.exp(1j * rng.uniform(0, 2 * np.pi, (N_DOPPLER, N_CODE_PHASE)))
ref2d = (
    np.sqrt(N) * np.fft.ifft2(spec)
).astype(np.complex64)

# Signal amplitude from SNR definition: snr_amp = A * sqrt(N) / SIGMA
A = snr_amp * SIGMA / math.sqrt(N)

det = Detector2D(
    ref2d, dwell=M, noise_lo=1, noise_hi=N - 1, threshold=0.0
)
for *_, stat in det.push(signal_block):
    detected = stat > theta
```

`Detector2D.push()` accepts arbitrary-length blocks and yields
`(row, col, peak_mag, noise_est, test_stat)` for each dwell; compare
`test_stat` against `theta` to declare acquisition.

```bash
python src/doppler/examples/detection2d_demo.py  # → detection2d_demo.png  (~10 s)
```

See [2-D Acquisition Grid](../examples/python-detection2d.md) for
the full demo including ROC construction and MC validation.
