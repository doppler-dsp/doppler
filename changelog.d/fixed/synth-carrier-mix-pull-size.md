- **A synth's output no longer depends on how the caller pulls it.** The
    carrier mix runs through one fixed-size block and `wfm_synth_core.c` is
    built with `-ffp-contract=off`, so no FMA or FCMLA compiler rounds a
    vector body and its tail differently. Costs +2.0% to +6.9% on 12 of 18
    `wfm_synth` bench-ab rows (6 tie). See [#2805](https://github.com/doppler-dsp/doppler/issues/2805).
