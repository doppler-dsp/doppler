# Python Utilities API

The `doppler.util` module holds small numeric helpers shared across the library:
`square_clip`, the per-component hard limiter used by the AGC and other
saturating paths, and `saturate`, the total range guard a loop puts on its own
state.

Source:
[`src/doppler/util/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/util/__init__.py)

______________________________________________________________________

## `square_clip`

Clips the real and imaginary parts of a complex sample **independently** to
`[-lin, lin]` — a **square** region in the IQ plane (each axis limited on its
own), as opposed to a circular magnitude clip. This is the cheap, branch-light
limiter a feedback loop applies after gain to bound excursions without rotating
the sample's phase quadrant.

```python
from doppler.util import square_clip

square_clip(3 + 4j, 1.0)    # (1+1j)   -> re and im each clamped to [-1, 1]
square_clip(0.5 - 0.2j, 1.0)  # (0.5-0.2j) -> inside the square, unchanged
```

Contrast with a circular clip, which would scale `3 + 4j` (magnitude 5) down to
magnitude `lin` while preserving its phase; `square_clip` instead clamps each
axis, which is what a fixed-point I/Q datapath does at its rails.

______________________________________________________________________

## `saturate`

Confines a value to `[lo, hi]` and is **total over every double** — including
NaN and both infinities. That totality is the point: `fmin(fmax(v, lo), hi)`
looks equivalent and is not, because every comparison against NaN is false, so
a NaN falls through to whichever bound the platform's `fmin` happens to return.

The NaN destination is therefore a **parameter**, not a default, because which
end is safe is domain knowledge rather than arithmetic:

```python
from doppler.util import saturate

saturate(0.5, 0.0, 1.0, 1.0)            # 0.5  -> inside, passed through
saturate(float("inf"), 0.0, 1.0, 1.0)   # 1.0  -> infinity is just above
saturate(float("nan"), 0.0, 1.0, 1.0)   # 1.0  -> a level: unknown reads LOUD
saturate(float("nan"), 0.0, 1.0, 0.0)   # 0.0  -> a lock stat: unknown is UNLOCKED
```

Use it where an untrusted value first becomes **persistent state** — the input
of an EMA, an accumulator or an integrator. Ahead of that boundary a bad value
corrupts one output and is gone; past it, it is remembered and everything
derived from it inherits the damage. The AGC's detector is the worked example:
see [Automatic Gain Control](../design/agc.md), whose §4 records what a single
non-finite sample did before this guard existed.

______________________________________________________________________

::: doppler.util.square_clip

::: doppler.util.saturate

## Related pages

<!-- related-pages:start -->

**Design** — [Automatic Gain Control](../design/agc.md)

<!-- related-pages:end -->
