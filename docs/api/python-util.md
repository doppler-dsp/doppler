# Python Utilities API

The `doppler.util` module holds small numeric helpers shared across the library.
Today it exposes one function, `square_clip` — the per-component hard limiter
used by the AGC and other saturating paths.

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

::: doppler.util.square_clip
