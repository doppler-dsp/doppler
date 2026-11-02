- **wfm_synth refuses a forged `sym_pos` or `sym_read_idx`, and checks
    before it writes.** A refused blob used to leave 13 fields restored; the
    synth now decodes, checks the whole set (the DSSS window included), then
    commits. See [#2142](https://github.com/doppler-dsp/doppler/issues/2142).
