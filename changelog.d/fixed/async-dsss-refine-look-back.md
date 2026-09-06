- **The async-DSSS receiver's C tests and harnesses ran the refine on a
    retired look-back** (`refine_max_error_db` 100 dB: one dump per epoch,
    which aliases the data lobe and keeps a third of the seed's Doppler
    error). The new `validate_refine_bias` measures the estimate on both
    streams (design §12.10); every caller is on the shipped 0.5 dB.
    [#1252](https://github.com/doppler-dsp/doppler/issues/1252).
