# Python Loop Filter API

The `doppler.track` module provides `LoopFilter` — a **second-order
proportional-integral loop filter**, the shared engine of every tracking loop
(Costas/PLL, DLL, symbol timing). An error `e` goes in, a control value comes
out (`control = integ + kp·e`), and the integrator advances `integ += ki·e`, so
the integrator holds the running frequency/rate estimate and `kp·e` is the
instantaneous (phase) correction.

Source:
[`src/doppler/track/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/track/__init__.py)

See the [DSSS despreader gallery page](../gallery/dsss-despread.md), which uses
two of these — one for the carrier loop, one for the code loop.

______________________________________________________________________

## How it works

The gains are derived from a loop **noise bandwidth** `bn` (normalized,
cycles/sample), a damping factor `zeta` (0.707 = critically damped), and the
update period `t` (samples):

```
wn = 8·zeta·bn / (4·zeta² + 1)
theta = wn·t
kp = 8·zeta·theta / (4 + 4·zeta·theta + theta²)
ki = 4·theta²     / (4 + 4·zeta·theta + theta²)
```

`configure(bn, zeta, t)` recomputes the gains while preserving the integrator
(so a tracker can retune mid-stream without losing lock); `reset()` zeroes the
integrator. The state struct is public C, so trackers embed it **by value** and
drive it with the same kernel — there is no per-update allocation.

______________________________________________________________________

## Examples

### Drive a loop with a constant error

```python
from doppler.track import LoopFilter

lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)
lf.step(1.0)                 # integ += ki; returns integ + kp
round(lf.integ, 6)           # == ki (one update of unit error)
```

### Retune without losing the estimate

```python
lf.configure(0.05, 0.707, 1.0)   # wider bandwidth; integ preserved
lf.reset()                       # zero the integrator
```

______________________________________________________________________

::: doppler.track.LoopFilter
