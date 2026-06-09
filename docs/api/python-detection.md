# Python Detection Statistics API

The `doppler.detection` module is the **detection-theory** layer over the C
`detection` core: closed-form relationships between probability of detection
(`Pd`), probability of false alarm (`Pfa`), SNR, and coherent dwell length for a
square-law detector. Pair it with the streaming
[`Detector`](python-spectral.md#streaming-detection) — `detection` tells you
*what threshold and dwell to use*, `Detector` *runs* the detection.

Every quantity comes in two forms: an **amplitude-SNR** version (`det_*`, SNR in
dB) and a **power-SNR** version (`det_*_power`, linear power ratio).

```python
from doppler.detection import det_threshold, det_pd, det_dwell, marcum_q

# Threshold for a target false-alarm rate, then Pd at a given SNR + dwell.
thr = det_threshold(pfa=1e-6)
pd  = det_pd(snr=10.0, dwell=8, threshold=thr)     # dB SNR

# How many frames must I integrate to reach Pd ≥ 0.9 at this Pfa?
n = det_dwell(snr=6.0, pd_min=0.9, pfa=1e-6, max_dwell=4096)

# The underlying Marcum Q-function is exposed directly.
q = marcum_q(1, 2.0, 3.0)
```

______________________________________________________________________

## Amplitude-SNR (dB)

::: doppler.detection.det_threshold

::: doppler.detection.det_pd

::: doppler.detection.det_dwell

::: doppler.detection.det_snr

______________________________________________________________________

## Power-SNR (linear)

::: doppler.detection.det_threshold_power

::: doppler.detection.det_pd_power

::: doppler.detection.det_dwell_power

::: doppler.detection.det_snr_power

______________________________________________________________________

## Primitive

::: doppler.detection.marcum_q
