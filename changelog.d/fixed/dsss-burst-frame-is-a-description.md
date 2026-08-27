- **A coding stage asked for on a DSSS burst was silently dropped**
    ([#1017](https://github.com/doppler-dsp/doppler/issues/1017)). `--conv`,
    `--asm`, `--rs-depth` and `--randomise` parsed, set their field, and were
    never read: a DSSS burst assembled its frame through a private four-field
    builder that had never heard of a stage, so `--conv` produced a
    byte-identical waveform to no `--conv` at all. The burst is now assembled
    from the same `wfm_frame_desc_t` every other source's frame is — and its
    record carries the stages, so a coded capture replays as itself instead of
    as a plausible uncoded one.
