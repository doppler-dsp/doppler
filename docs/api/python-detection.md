# Python Detection Statistics API

The `doppler.detection` module is the **detection-theory** layer over the C
`detection` core: closed-form relationships between probability of detection
(`Pd`), probability of false alarm (`Pfa`), SNR, and coherent dwell length for a
square-law detector. Pair it with the streaming
[`CorrDetector`](python-spectral.md#streaming-detection) — `detection` tells you
*what threshold and dwell to use*, `CorrDetector` *runs* the detection.

Every quantity comes in two forms: an **amplitude-SNR** version (`det_*`, where
SNR is the *linear* signal/noise **amplitude** ratio) and a **power-SNR** version
(`det_*_power`, the linear **power** ratio = amplitude²). The two are equivalent
detectors — `det_pd(s, ...)` equals `det_pd_power(s**2, ...)`.

The threshold depends only on the target false-alarm rate; `Pd` then depends on
the SNR and the coherent dwell. The whole chain is closed-form and stateless:

```pycon
>>> from doppler.detection import det_threshold, det_pd, det_dwell, det_snr
>>> thr = det_threshold(pfa=1e-6)               # threshold for Pfa = 1e-6
>>> round(thr, 4)
5.2565
>>> round(det_pd(snr=1.613, dwell=8, threshold=thr), 2)
0.9
>>> det_dwell(snr=0.5, pd_min=0.9, pfa=1e-6, max_dwell=256)
84
>>> round(det_snr(dwell=8, pd_min=0.9, pfa=1e-6), 3)  # inverse of det_pd
1.613

```

The underlying Marcum Q-function is exposed directly — under H0 (`a = 0`) it is
the Rayleigh tail `exp(-b²/2)`:

```pycon
>>> from doppler.detection import marcum_q
>>> round(marcum_q(m=1, a=0.0, b=1.0), 5)  # P(Rayleigh > 1) = exp(-0.5)
0.60653
>>> round(marcum_q(m=1, a=2.0, b=1.0), 5)  # signal present (a = 2)
0.91811

```

______________________________________________________________________

## Amplitude-SNR (dB)

::: doppler.detection.det_threshold

::: doppler.detection.det_pd

::: doppler.detection.det_dwell

::: doppler.detection.det_snr

______________________________________________________________________

## Non-coherent integration

When coherent integration is capped (Doppler walk, data bits, oscillator drift,
Doppler rate), `N_nc` coherent **looks** are combined by summing squared
magnitude. The detector becomes the order-`N_nc` Marcum-Q; these helpers package
it and reduce to the coherent (order-1) versions above at `n_noncoh = 1`. They
drive the `doppler.dsss.Acquisition` engine's coherent/non-coherent split.

::: doppler.detection.det_threshold_noncoherent

::: doppler.detection.det_pd_noncoherent

::: doppler.detection.det_n_noncoh

______________________________________________________________________

## Estimator smoothing

`det_ema_alpha` sizes a first-order EMA probabilistically: treat the
quantity being smoothed as a DC level in noise with a per-sample
estimator SNR (mean² / variance), pick the output SNR the decision
needs, and the coefficient follows from the EMA's variance reduction
`(2 − α)/α`. It is how the DLL's code-lock detector sizes its CFAR
noise-reference bandwidth (`Dll.configure_lock(..., ref_snr_db=...)`),
and the same call sizes any lock-metric smoother when the per-look SNR
is known from C/N0:

```python
from doppler.detection import det_ema_alpha

# signal-free power reference: exponential samples = 0 dB per sample;
# a 20 dB estimator SNR needs an ~50-look EMA
assert round(1 / det_ema_alpha(0.0, 20.0), 1) == 50.5

# only the requested gain matters, not where the pair sits in dB
assert abs(det_ema_alpha(10.0, 30.0) - det_ema_alpha(0.0, 20.0)) < 1e-15

# already good enough -> no averaging
assert det_ema_alpha(6.0, 3.0) == 1.0
```

::: doppler.detection.det_ema_alpha

______________________________________________________________________

## Lock verification

A loop that computes a lock statistic still needs a *decision rule*: when is
the statistic high enough, long enough, to declare lock — and low enough,
long enough, to drop it? `LockDet` is that rule factored out once: separate
declare/drop thresholds (level hysteresis) plus consecutive-look verify
counts (time hysteresis). Consecutive looks compound probabilistically —
`n` looks at per-look probability `p` reach `p^n` — so the verify counts are
*derived*, not guessed: `det_verify_count` sizes them from a per-look rate
and a compound budget, and `det_verify_delay` predicts the declare latency
they cost. The DLL's code-lock latch and the M-PSK receiver's two-way
acquisition↔tracking handover both run on an embedded C `lockdet`.

```python
from doppler.detection import LockDet, det_verify_count, det_verify_delay

# declare side: per-decision pfa 1e-3, false-declare budget 1e-9 -> 3 straight
n_up = det_verify_count(1e-3, 1e-9)
assert n_up == 3

# drop side: per-look miss rate 1-pd = 0.2, false-drop budget 1e-4 -> 6
n_down = det_verify_count(0.2, 1e-4)
assert n_down == 6

# the price in latency: mean looks to a declare at pd = 0.9
assert round(det_verify_delay(0.9, n_up), 2) == 3.72

d = LockDet(up_thresh=8.5, down_thresh=7.0, n_up=n_up, n_down=n_down)
assert [d.step(9.0), d.step(9.0), d.step(9.0)] == [0, 0, 1]  # 3rd hit locks
assert d.step(7.5) == 1  # inside the hysteresis band: sticky
```

::: doppler.detection.det_verify_count

::: doppler.detection.det_verify_delay

::: doppler.detection.LockDet

______________________________________________________________________

## Power-SNR (linear)

::: doppler.detection.det_threshold_power

::: doppler.detection.det_pd_power

::: doppler.detection.det_dwell_power

::: doppler.detection.det_snr_power

______________________________________________________________________

## Primitive

::: doppler.detection.marcum_q
