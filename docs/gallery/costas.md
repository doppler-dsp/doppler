# Carrier Loop Stress

![Costas carrier-loop stress demo](../assets/costas_demo.png)

A [`track.Costas`](../api/python-track.md) carrier-tracking loop pulling in and
holding a **residual** carrier offset that *moves*. After FFT acquisition has
removed the bulk Doppler, the loop sees only a small residual — here a
2-millicycle/sample frequency **step** plus a slow Doppler **ramp** — at
`SNR = 15 dB`. The same scenario runs at three loop noise bandwidths so the
tracking trade-off is visible.

## What you're seeing

**Top — Frequency tracking.** Each loop's integer-NCO frequency estimate riding
the true moving residual (black dashed). The wide loop (`Bn = 0.12`) snaps onto
the step; the narrow loop (`Bn = 0.03`) approaches slowly with visible
overshoot/ringing before settling. All three then follow the ramp.

**Middle — Loop stress vs time.** The sliding-RMS of the Costas phase
discriminator error (degrees, log scale) — the instantaneous *stress* on the
loop. A large **transient** stress during pull-in decays to a low steady
**floor**. Wider `Bn` clears the transient fastest (shortest acquisition);
narrower `Bn` rings longer. Once locked, all settle near the same floor (here
noise-dominated, since the ramp's dynamic stress is small).

**Bottom — Lock metric vs time.** `|Re P| / |P|` ramping to 1 as each loop
locks. The narrow loop's ringing acquisition shows up as the metric dipping and
recovering before it finally locks.

## How it works

The carrier loop is one small primitive composed from two others:

- [`source.LO`](../api/python-nco.md) — an **integer-phase NCO** (uint32
    accumulator + LUT → cf32). The phase wraps at 2³² by construction, so it is
    bounded and exactly reproducible — no `double`-accumulator drift over long
    runs. The loop de-rotates the input one sample at a time with the inline
    `lo_step()` (carrier wipe-off).
- [`track.LoopFilter`](../api/python-track.md) — the 2nd-order PI loop filter
    that turns the per-symbol phase error into a frequency + phase steer.

Each `tsamps`-sample symbol is coherently integrated (integrate-and-dump), a
decision-directed BPSK discriminator measures the residual phase, the loop
filter updates, and the new frequency/phase is written straight into the NCO.

```python
import numpy as np
from doppler.track import Costas

# residual carrier offset f0 (cycles/sample), tsamps samples per symbol
c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=16)
symbols = c.steps(rx)          # one complex prompt per symbol
f_est   = c.norm_freq          # tracked residual frequency
locked  = c.lock_metric        # |Re|/|P| EMA, 1.0 when phase-locked
```

`Costas` tracks only the *residual* left after acquisition; an offset larger
than the per-symbol integration bandwidth must be removed upstream by the FFT
acquisition search, not by the loop.

Source: `src/doppler/examples/costas_demo.py`.
