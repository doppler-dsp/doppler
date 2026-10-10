- **`AWGN`'s state blob carries its seed and amplitude (v2, 60 B, from
    304 B), and refuses a bad amplitude or an all-zero RNG state.** A
    restored generator keeps the source's amplitude and the seed `reset()`
    replays, so a restored `wfm` synth emits the source's noise level (#2084).
    NaN, infinite and negative amplitudes are refused at create and on
    restore; the setter ignores them, and the one-shot `dp_awgn` returns
    `DP_ERR_INVALID` for them.
