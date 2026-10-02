- **A synth pulling from a data source now checkpoints and resumes bit for
    bit.** The data source has its own state triplet (a Field's cursor,
    `pn:0`'s register, a file's residue and running hash, the prefix
    re-checked on restore), and the synth blob (v3) nests it beside the frame
    in play. A pipe refuses at both ends with a static reason
    (`dp_wfm_data_state_refusal`). C only for now: no Python face serializes
    a data-backed synth yet
    ([#1780](https://github.com/doppler-dsp/doppler/issues/1780)).
    ([#1681](https://github.com/doppler-dsp/doppler/issues/1681))
