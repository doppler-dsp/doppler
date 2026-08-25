# Detection Theory Curves

![Detection theory curves](../assets/detection_curves.png)

## What you're seeing

**Left — Pd vs dwell M** at Pfa = 1e-5, for SNR = 0, 3, 6, 10 dB.
Curves shift left as SNR increases: more per-sample SNR trades
against coherent integration depth. Filled circles mark where each
curve first crosses Pd = 0.9 — M = 18, 9, 5, 2.

**Middle — minimum dwell for Pd ≥ 0.9 vs SNR** at fixed Pfa = 1e-5.
Every 3 dB of extra SNR roughly halves the required dwell: at 0 dB
you need 18 dwells for Pd = 0.9; at +6 dB you need only 5.

**Right — what an ESTIMATED noise reference costs.** The two panels
beside it price a statistic normalised by a *known* noise power. A
burst detector does not have one: it has a single burst, and estimates
the noise from that same burst. The exact null law is then
`R² = n·F(n, n)`, whose tail is fatter than the chi-square gate
prices — so a detector sold at one false alarm in a thousand delivers
one in twenty-four at n = 16. `det_threshold_f` is the gate that
removes it. The penalty shrinks as the estimate hardens (127× at
n = 2, 22× at n = 128) and never becomes negligible over any dwell a
burst affords.

Two things worth knowing before you use that panel. The comparator is
`det_threshold_noncoherent(pfa, n/2)`, **not** `(pfa, n)`: the
statistic carries `n` real degrees of freedom, one per prompt, while
that helper prices `chi2(2M)` because a non-coherent look is a complex
magnitude and carries two. Pricing it at `n` yields 4.8× — a plausible
number, and wrong by almost ten. And the 41× is not a recollection: it
is re-derived here from the shipped API, pinned in
`native/tests/test_detection_core.c`, and independently confirmed at
**41.1×** by two million Monte-Carlo draws in the `models`
characterization sweep.

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
detectors, [Detection Sizing](../design/detection.md) for why five
different laws ship under one `det_` prefix and what happens when they
are crossed, and the
[certification report](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/detection/tests/validation/detection/results.md)
for the certified envelope these curves sit inside.
