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

### How long until it has settled

`settling_samples` answers the question a warm-up budget, a burst preamble
or an acquisition guard has to answer, and that `1/(4*loop_bw)` does not:

```python
from doppler.agc import settling_samples

settling_samples(0.0025, 0.05, 40.0, 0.5)   # 430 — cold, 40 dB quiet
settling_samples(0.0025, 0.05, -40.0, 0.5)  # 175 — loud, the fast direction
```

`1/(4*loop_bw)` is the loop **filter's** time constant, and the object is
not the filter: the detector sits inside the loop and measures in *power*,
so a quiet input settles more slowly. Pass the largest gain error you expect
to *start* from — **positive for a quiet input**, which is the slow
direction and the one to budget for. For a cold receiver that is the whole
input dynamic range it must cover, not the steady-state variation.

It runs the real loop and counts rather than evaluating a fitted curve, so
it cannot go stale relative to the object it describes. That makes it a
design-time call: it allocates and iterates, so use it while planning a
pipeline, never inside one. Invalid arguments return `0` rather than a
plausible-looking guess.

The multiplier it is measuring is charted across both design axes in
[AGC Settling — a design chart](../gallery/agc-settling-design.md).

______________________________________________________________________

::: doppler.agc.AGC

::: doppler.agc.settling_samples

## Related pages

<!-- related-pages:start -->

**Gallery** — [AGC Settling — a design chart](../gallery/agc-settling-design.md), [AGC — Step Response](../gallery/agc.md), [Gallery](../gallery/index.md)
**Design** — [API taxonomy: the DSP building-block hierarchy and its naming axis](../design/api-taxonomy.md), [MPSK Receiver](../design/mpsk.md)
**Contributing** — [Validation log](../dev/validation-log.md)

<!-- related-pages:end -->
