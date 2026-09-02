- **The full-band acquisition search never looked at its outermost native
    bin at an even coherent depth.** 1/D of a uniform Doppler prior was
    undetectable at any C/N0 — the hole the coarse-Doppler bank fell into
    between two channels. Fixed; the Pd model now derates scalloping over
    the interpolated bin the search samples (0.47 → 0.63 predicted, 0.72
    measured at D=8, 50 dB-Hz).
    [#1183](https://github.com/doppler-dsp/doppler/issues/1183),
    [#1179](https://github.com/doppler-dsp/doppler/issues/1179).
