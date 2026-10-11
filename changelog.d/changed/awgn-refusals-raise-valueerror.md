- **`AWGN` refuses a bad argument with a `ValueError`, not a `MemoryError`,
    and the C setter reports its refusal.** A `NaN`, infinite or negative
    `amplitude` given to the constructor used to report running out of memory.
    Code catching `MemoryError` around `AWGN(...)` must catch `ValueError` now.
    `dp_awgn_set_amplitude` changed from `void` to `int`: it returns `DP_OK`,
    or `DP_ERR_INVALID` for a refused value. Callers that drop the return keep
    their old sigma (#2084).
- **AWGN state blobs written before v2, and `wfm_synth` blobs that embed one,
    no longer restore.** A v1 `AWGN` blob is refused; a noisy synth blob
    written since v0.61 embeds one and is refused as a length error. The synth
    blob version is unchanged (3); the owner decides whether to bump the synth
    version, and the refusal is the record until then.
