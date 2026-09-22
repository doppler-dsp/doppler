- **The acquisition engine reads a preamble's shape, not its chip count.**
    The peak zone, the twin rule and the Pd model's delay straddle now come
    from a per-waveform `acq_shape_t`. A PN code fills it analytically, so
    every existing engine is bit-identical: 318 configurations, every field,
    hit and blob. This is phase 1 of
    [#1470](https://github.com/doppler-dsp/doppler/issues/1470), acquiring
    any repeated complex64 preamble.
