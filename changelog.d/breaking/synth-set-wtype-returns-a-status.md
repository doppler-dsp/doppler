- **`dp_wfm_synth_set_wtype` returns `int` and refuses a type it cannot
    run.** A type outside the enum, or one whose generator `create()` did
    not build, is `DP_ERR_INVALID` with the type unchanged. In Python,
    `_SynthEngine.set_wtype` raises `ValueError` and takes the keyword `val`
    (was positional-only `value`); a v5 synth blob is refused by its version.
    See [#2095](https://github.com/doppler-dsp/doppler/issues/2095).
