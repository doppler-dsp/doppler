# Monte Carlo vs Marcum Q Theory

![Monte Carlo vs theory](../assets/detection_sim.png)

## What you're seeing

**Left — empirical survival functions vs closed-form curves.** Each
histogram is built from 30,000 independent trials per SNR point. The
solid lines are the Rician (H1) and Rayleigh (H0) CDFs evaluated at
the same parameters. Empirical and theoretical curves sit on top of
each other throughout — no visible deviation.

**Right — Pd vs SNR.** Monte Carlo operating points (dots) track the
Marcum Q prediction (solid line) within statistical noise at every
tested SNR. The Pfa operating point is confirmed by the fraction of
H0 trials that exceed the threshold.

## How it works

The simulation draws matched-filter outputs directly from their
theoretical distributions rather than running the full FFT pipeline:

- **H0**: envelope ~ Rayleigh(`σ`) — noise-only hypothesis
- **H1**: envelope ~ Rician(`ν`, `σ`) — signal + noise

This avoids the degeneracy that appears when a single-bin tone is
correlated against itself via FFT (the signal component is
deterministic and the test statistic is not representative of the
general case).

```python
--8<-- "src/doppler/examples/detection_sim.py:mc"

--8<-- "src/doppler/examples/detection_sim.py:sweep"
```

The Rician draw uses a single non-central Gaussian pair:
`re0 ~ N(sig_amp, sigma_ac)`, `im0 ~ N(0, sigma_ac)`. The envelope
`hypot(re0, im0)` is exactly Rician-distributed.

```bash
python src/doppler/examples/detection_sim.py   # → detection_sim.png  (~5 s)
```

See [Detection Theory Curves](detection-curves.md) for the closed-form
`det_pd` / `det_dwell` / `det_threshold` reference and threshold
derivation.
