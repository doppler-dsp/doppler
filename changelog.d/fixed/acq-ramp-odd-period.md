- **A tiled `Acquisition` with a carrier set ramps an odd period's middle
    bin with the right sign.** The tile path's code-rate ramp took bin
    `(N-1)/2` of an odd period `N` as negative, `-(N+1)/2`, so that one bin
    of each ramped tile's cross-spectrum had the wrong phase. It now runs
    the engine's one ramp, which follows numpy's `fftfreq`; an even period
    is unchanged bit for bit (#2815).
