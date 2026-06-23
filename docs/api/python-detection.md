# Python Detection Statistics API

The `doppler.detection` module is the **detection-theory** layer over the C
`detection` core: closed-form relationships between probability of detection
(`Pd`), probability of false alarm (`Pfa`), SNR, and coherent dwell length for a
square-law detector. Pair it with the streaming
[`Detector`](python-spectral.md#streaming-detection) — `detection` tells you
*what threshold and dwell to use*, `Detector` *runs* the detection.

Every quantity comes in two forms: an **amplitude-SNR** version (`det_*`, where
SNR is the *linear* signal/noise **amplitude** ratio) and a **power-SNR** version
(`det_*_power`, the linear **power** ratio = amplitude²). The two are equivalent
detectors — `det_pd(s, ...)` equals `det_pd_power(s**2, ...)`.

The threshold depends only on the target false-alarm rate; `Pd` then depends on
the SNR and the coherent dwell. The whole chain is closed-form and stateless:

```pycon
>>> from doppler.detection import det_threshold, det_pd, det_dwell, det_snr
>>> thr = det_threshold(1e-6)        # threshold for Pfa = 1e-6
>>> round(thr, 4)
5.2565
>>> round(det_pd(1.613, 8, thr), 2)  # amplitude SNR 1.613, 8-sample dwell
0.9
>>> det_dwell(0.5, 0.9, 1e-6, 256)   # dwell to reach Pd>=0.9 at SNR 0.5
84
>>> round(det_snr(8, 0.9, 1e-6), 3)  # inverse: min SNR for Pd>=0.9 at dwell 8
1.613

```

The underlying Marcum Q-function is exposed directly — under H0 (`a = 0`) it is
the Rayleigh tail `exp(-b²/2)`:

```pycon
>>> from doppler.detection import marcum_q
>>> round(marcum_q(1, 0.0, 1.0), 5)  # P(Rayleigh > 1) = exp(-0.5)
0.60653
>>> round(marcum_q(1, 2.0, 1.0), 5)  # signal present (a = 2)
0.91811

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
