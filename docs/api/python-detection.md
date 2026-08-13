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

## Gaussian test statistic

The helpers above size the amplitude-ratio detector, whose H0 law is Rayleigh.
A second family of detectors thresholds a statistic that is **Gaussian** under
H0 — a lock metric averaged over enough looks for the CLT to hold. Those share
one sizing chain, used by `SymbolSync`'s timing lock and the carrier lock
detectors. The rationale, the measured evidence, and the independence
assumption it rests on are in
[the design note](../design/lock-detect.md).

Given the statistic's H0 variance and its H1 mean at the operating point,
`det_dwell_gauss` gives the looks needed and `det_threshold_gauss` the declare
threshold; both are expressed in `det_q_inv`, the standard-normal upper tail.

!!! warning "`det_threshold` is the wrong law here"

    It inverts the envelope law and returns 4.7985 at `pfa = 1e-5`, where
    `det_q_inv` returns 4.2649. Only one of those is a sigma count for a
    zero-mean Gaussian.

```python
from doppler.detection import (
    det_dwell_gauss,
    det_q_inv,
    det_threshold_gauss,
)

# A statistic with H0 variance 1/2 and an H1 mean of 0.635 at the design
# point, wanted at pd = 0.99 with a 1e-5 per-look false-alarm budget.
assert det_dwell_gauss(mean=0.6349, var=0.5, pd=0.99, pfa=1e-5) == 54
assert round(det_threshold_gauss(mean=0.6349, pd=0.99, pfa=1e-5), 4) == 0.4108

# The quantile is SIGNED: negative above the median, which is why the
# separation below is a sum of two tails rather than a difference.
assert round(det_q_inv(p=1e-5), 4) == 4.2649
assert round(det_q_inv(p=0.99), 4) == -2.3263
```

::: doppler.detection.det_q_inv

::: doppler.detection.det_dwell_gauss

::: doppler.detection.det_threshold_gauss

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
counts (time hysteresis). Consecutive **independent** looks compound
probabilistically — `n` looks at per-look probability `p` reach `≈ p^n` — so
the verify counts are *derived*, not guessed: `det_verify_count` sizes them
from a per-look rate and a compound budget, and `det_verify_delay` predicts
the declare latency they cost. Both the `≈` and *independent* are load-bearing
enough to have their own section in
[the design note](../design/lock-detect.md). The DLL's code-lock latch and the M-PSK receiver's two-way
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

One-shot (burst) decisions use the same machinery with one twist: when
the noise reference is *estimated from as many samples as the signal
sum* (the `BurstDespreader` lock test), the exact H0 law is F(n, n),
not chi-square — `det_threshold_f` prices that gate exactly, for every
`n`:

```python
from doppler.detection import det_threshold_f

# F(2,2) tail is 1/(1+g): the quantile is exactly (1-pfa)/pfa
assert round(det_threshold_f(1e-3, 2), 6) == 999.0

# the estimate hardens with dof: the gate approaches the known-noise one
assert det_threshold_f(1e-3, 16) > det_threshold_f(1e-3, 64) > 1.0
```

::: doppler.detection.det_threshold_f

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

## Related pages

<!-- related-pages:start -->

**Gallery** — [Streaming Async Despreader](../gallery/async-despread.md), [Measuring an Error Rate, Defensibly](../gallery/ber-awgn.md), [CarrierAcquisition: RRC Pulse Shaping](../gallery/carrier-acq-rrc.md), [Detection Theory Curves](../gallery/detection-curves.md), [Monte Carlo vs Marcum Q Theory](../gallery/detection-sim.md), [Lock Detection: Verify Counts + Hysteresis](../gallery/lockdet.md), [M-PSK Receiver — Pull-in, Lock, and BER](../gallery/mpsk-receiver.md)
**Guides** — [DSSS Burst Acquisition](../guide/dsss-acquisition.md), [Lock Detection Across `doppler.track`](../guide/lock-detection.md)
**Design** — [Asynchronous symbol/code despreading](../design/async-symbol-despreader.md), [DSSS acquisition: stateless, parallel, dynamics-capable](../design/dsss-acquisition.md), [The Exponential Moving Average](../design/ema.md), [MPSK Receiver](../design/mpsk.md), [SymbolSync Timing Lock Detector](../design/timing_lock_detector.md)

<!-- related-pages:end -->
