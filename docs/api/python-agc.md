# Python AGC API

The `doppler.agc` module is a **log-domain feedback automatic gain control** for
complex baseband. It drives the average output power to a target (`ref_db`) by
integrating the power error in dB, so convergence is exponential and independent
of the absolute input level. The loop is decimated — the detector and integrator
run once per `decim` samples with a first-order hold on the gain between updates
— so a long block costs `O(n/decim)` control work, not `O(n)`.

Source:
[`src/doppler/agc/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/agc/__init__.py)

See the [AGC gallery page](../gallery/agc.md) for convergence plots and the
attack/decay behaviour under bursts.

______________________________________________________________________

## How it works

Three constructor parameters tune the closed loop:

- **`ref_db`** — the target average output power (dB). The integrator starts at
    0 dB (unity gain) and the detector is pre-seeded to `ref_db`, so an
    on-target first block produces no transient.
- **`loop_bw`** — normalised loop bandwidth; larger converges faster but tracks
    noisier.
- **`alpha`** — the power detector's EMA smoothing factor.

A steady input of magnitude `A` settles to a gain of `ref_db − 20·log10(A)` dB,
bringing the output to the target. The current loop state is readable through
`gain_db` (the loop integrator) and `applied_gain_db` (the gain actually applied
to the most recent sample after the first-order hold).

______________________________________________________________________

## Examples

### Converge a steady signal to the target

```python
import numpy as np
from doppler.agc import AGC

agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)

# A constant-magnitude-4 tone is 12 dB hot; the loop pulls it to unity.
x = np.full(2000, 4.0 + 0j, dtype=np.complex64)
y = agc.steps(x)

round(agc.gain_db, 1)        # -12.0  (settled gain)
round(abs(y[-1]), 3)         # ~1.0   (output at the 0 dB target)
```

### Process a new segment from a clean state

`reset()` returns the loop to its post-construction condition (unity gain,
detector re-seeded from `ref_db`) without re-allocating.

```python
agc.reset()
agc.gain_db, agc.applied_gain_db   # (0.0, 0.0)
next_segment = np.full(2000, 2.0 + 0j, dtype=np.complex64)
y2 = agc.steps(next_segment)
```

### In-place

`steps` may write into the input buffer (the output array can alias the input):

```python
buf = np.full(2000, 2.0 + 0j, dtype=np.complex64)
agc.steps(buf, out=buf)
```

______________________________________________________________________

::: doppler.agc.AGC

## Related pages

<!-- related-pages:start -->

**Gallery** — [AGC — Step Response](../gallery/agc.md), [Gallery](../gallery/index.md)
**Design** — [API taxonomy: the DSP building-block hierarchy and its naming axis](../design/api-taxonomy.md), [State Serialization — the standard bytes interface](../design/state-serialization.md)

<!-- related-pages:end -->
