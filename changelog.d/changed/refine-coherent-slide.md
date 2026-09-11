- **`BurstCapture` refine picks the repetition with a coherent slide over a
    Doppler search, not a non-coherent boxcar.** The gain widens with depth,
    because coherent combining buys `10*log10(reps)` where non-coherent buys
    `5*log10(reps)`: swept at 300 trials a point, the correct-repetition rate
    at 39 dB-Hz went 0.59 -> 0.70 at `reps=5`, 0.60 -> 0.81 at 10 and
    0.54 -> 0.80 at 16. The slow-time search covers acquisition's residual by
    construction, so there is nothing to tune. See
    [#1312](https://github.com/doppler-dsp/doppler/issues/1312).
