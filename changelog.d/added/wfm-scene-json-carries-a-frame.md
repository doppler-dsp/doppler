- **The scene JSON carries a caller's frame, both ways.** A `frame` key on a
    source round-trips a `wfm_frame_desc_t` through `--record` /
    `--from-file` — and through `Composer.to_json` / `from_json`, which are
    the same C code, so **Python reaches a carried frame without any new
    binding**. A stage's kind is written as its name when doppler has one and
    as its integer when it does not, which is the only encoding that can carry
    a caller's own kind from `WFM_STAGE_USER` up. Closes
    [#1140](https://github.com/doppler-dsp/doppler/issues/1140).
