# AGC — Step Response

![AGC convergence](../assets/agc_convergence.png)

## What you're seeing

A 6000-sample complex tone that steps from −10 dBm to +10 dBm at sample
3000\. Three curves overlay almost perfectly because all three use the
same `loop_bw = 0.00125`; only the decimation factor changes how often
the loop ticks.

- **decim=1** — loop updates every sample; fastest per-sample cost.
- **decim=8** — loop updates every 8 samples; ×8 cheaper, identical
    trajectory.
- **decim=16** — coarsest timing; still converges within ~350 samples
    of the 20 dB step.

The gain trace is output power in dBFS. All three curves converge to
0 dBFS before the step and recover to 0 dBFS within ~350 samples
after it.

## How it works

`agc_steps()` rescales the loop coefficients by `decim` so that
`loop_bw` keeps its per-sample meaning regardless of how coarsely
the detector ticks.

```python
--8<-- "src/doppler/examples/agc_demo.py:step_response"
```

`alpha` controls the exponential moving-average window for the power
detector; `loop_bw` sets the first-order loop bandwidth. Wider
`loop_bw` → faster tracking but more noise at steady state. Read the
converged gain back off the object:

```python
print(f"commanded gain : {agc.gain_db:+.2f} dB")
print(f"applied gain   : {agc.applied_gain_db:+.2f} dB")
# commanded gain : -10.00 dB
# applied gain   : -10.00 dB
```

## Per-sample decimation — `gain_update_period`

The same "tick the loop less often" trick is available on the
**per-sample** `step()` path via `gain_update_period` (default `1` = the
exact per-sample loop). With `gain_update_period = P > 1`, `step()` still
applies the gain and folds the power detector every sample, but refreshes
the loop-filter command (the `exp10`/`log10` work) once per `P` samples —
a zero-order hold that amortises the transcendentals on a sample-rate hot
loop, without the block latency of `steps()`. It is the streaming analogue
of `decim`, for an AGC embedded inside a per-sample feedback loop (e.g. the
[NDA carrier loop](../design/mpsk.md)'s arm) that cannot tolerate block
buffering. As with `decim`, `loop_bw` keeps its per-sample meaning.

```bash
python src/doppler/examples/agc_demo.py   # → agc_convergence.png
```
