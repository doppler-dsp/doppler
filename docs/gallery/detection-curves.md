# Detection Theory Curves

![Detection theory curves](../assets/detection_curves.png)

## What you're seeing

**Left — Pd vs dwell M** at Pfa = 1e-5, for SNR = 0, 3, 6, 10 dB.
Curves shift left as SNR increases: more per-sample SNR trades
against coherent integration depth. Filled circles mark where each
curve first crosses Pd = 0.9 — M = 18, 9, 5, 2.

**Right — minimum dwell for Pd ≥ 0.9 vs SNR** at fixed Pfa = 1e-5.
Every 3 dB of extra SNR roughly halves the required dwell: at 0 dB
you need 18 dwells for Pd = 0.9; at +6 dB you need only 5.

## How it works

`det_pd`, `det_dwell`, and `det_threshold` implement the closed-form
Marcum Q functions. No simulation is needed to set a threshold or
predict performance:

```python
--8<-- "src/doppler/examples/detection_curves.py:theory"

--8<-- "src/doppler/examples/detection_curves.py:checks"
```

`det_threshold` inverts the Rayleigh CDF at `Pfa` to get the CFAR
gate `eta`. `det_dwell` binary-searches over M until
`det_pd(snr, M, eta) >= pd_target`.

```bash
python src/doppler/examples/detection_curves.py   # → detection_curves.png
```

See [Monte Carlo vs Marcum Q](detection-sim.md) for the 30,000-trial
validation of these closed-form curves against the envelope and power
detectors.
