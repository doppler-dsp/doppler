- **A Plan renders what compose() composes on compilers that fuse
    multiply-adds.** The synth wrote `sig * carrier + noise` as one expression,
    which AArch64 GCC 16 fuses into a single rounding while a Plan caches the
    signal and adds the noise afterwards, so 263 of 2048 floats of a bundled
    source differed. The synth now stores the signal and adds its noise through
    `wfm_add_scaled`, the one accumulate the Plan and the composer use too, so
    the paths are the same instructions on every compiler. The `noise (AWGN)`
    bench row (the standalone noise generator) is about 4% slower for the
    second pass. See [#2789](https://github.com/doppler-dsp/doppler/issues/2789).
