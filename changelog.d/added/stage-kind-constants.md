- **`doppler.wfm` exports named stage kinds** — `STAGE_CRC16`, `STAGE_RS`,
    `STAGE_RANDOMISE`, `STAGE_CONV`, `STAGE_INTERLEAVE` and `STAGE_USER`,
    generated from `wfm_stage_kind_t` by `scripts/gen_stage_kinds.py`. Five
    hand copies of the numbering are deleted. `add_stage(kind=...)` stays an
    int because the kind is an open `uint32_t` a caller extends.
    [#1223](https://github.com/doppler-dsp/doppler/issues/1223).
