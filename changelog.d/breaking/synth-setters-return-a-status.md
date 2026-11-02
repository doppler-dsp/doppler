- **`dp_wfm_synth_set_nsps`/`set_sym_pos` return `int` and refuse a bad
    value.** A hold at or below the current `sym_pos` is
    `DP_ERR_INVALID`, so set `sym_pos` first. In Python,
    `_SynthEngine.set_nsps`/`set_sym_pos` raise `ValueError`, and their
    argument is the keyword `val` (was positional-only `value`). See [#2142](https://github.com/doppler-dsp/doppler/issues/2142).
