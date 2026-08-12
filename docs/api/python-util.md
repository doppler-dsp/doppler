# Python Utilities API

The `doppler.util` module holds small numeric helpers shared across the library:
`square_clip`, the per-component hard limiter used by the AGC and other
saturating paths; `saturate`, the total range guard a loop puts on its own
state; and `ema_step`/`ema_alpha_decim`, the library's one exponential moving
average and the coefficient that advances it a whole chunk at a time.

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

## `ema_step`

One step of the library's first-order exponential moving average,
`state + alpha * (x - state)`. Every running estimator in doppler is this
recursion with a different input — a power detector, a lock statistic, a
spectrum accumulator — and it exists as one function because four
hand-written copies, in two different algebraic forms, cannot be reasoned
about together.

```python
from doppler.util import ema_step

ema_step(0.0, 1.0, 0.5)     # 0.5  -> halfway to the observation
ema_step(2.0, 2.0, 0.25)    # 2.0  -> at its fixed point, exactly no motion
ema_step(1.0, 7.0, 1.0)     # 7.0  -> alpha 1 is EXACT pass-through
ema_step(1.0, 7.0, 0.0)     # 1.0  -> alpha 0 freezes the state
```

The two boundaries are contract, not tolerance. `alpha = 1` means "do not
average" and is a request callers really make — `det_ema_alpha(0, 0)` returns
exactly `1.0` — so it returns the observation bit-exactly rather than the few
ulps the bare recursion would give. `alpha = 0` freezes the state exactly, and
a coefficient above 1 saturates to pass-through instead of overshooting.

Like `saturate`'s companion note above, this function is deliberately **not
total in `x`**: a non-finite observation poisons the state permanently, because
an EMA remembers. That is why the guard belongs on this function's input. Why
this algebraic form and not the other, and what its noise reduction and time
constant are, is in
[The Exponential Moving Average](../design/ema.md).

______________________________________________________________________

## `ema_alpha_decim`

The coefficient that advances an EMA `d` samples in a single step,
`1 - (1 - alpha)^d`. A loop that updates once per chunk uses it so the
decimation factor stays a performance knob instead of a retune.

```python
from doppler.util import ema_alpha_decim, ema_step

ema_alpha_decim(0.05, 1)    # 0.05 -> d == 1 returns alpha EXACTLY
round(ema_alpha_decim(0.05, 8), 12)   # 0.336579568711

# d steps of alpha == one step of the compounded coefficient
s = 0.0
for _ in range(8):
    s = ema_step(s, 1.0, 0.05)
round(s - ema_step(0.0, 1.0, ema_alpha_decim(0.05, 8)), 15)   # 0.0
```

It is computed through `expm1`/`log1p` because the direct expression cancels:
at `d = 1`, where the answer must be `alpha` itself, `1 - (1 - alpha)` is 6 ulps
off at `alpha = 0.05` and 26865 ulps off at `1e-5`. Exactness at `d = 1` is what
makes a decimated path comparable to the undecimated one at all.

______________________________________________________________________

::: doppler.util.square_clip

::: doppler.util.saturate

::: doppler.util.ema_step

::: doppler.util.ema_alpha_decim

## Related pages

<!-- related-pages:start -->

**Design** — [Automatic Gain Control](../design/agc.md), [The Exponential Moving Average](../design/ema.md)

<!-- related-pages:end -->
