- **A synth's output no longer depends on how the caller pulls it.**
    `wfm_synth_core.c` is built with `-ffp-contract=off`, so an FMA compiler
    fuses no multiply-add differently in a loop's body and tail; it costs 3% to
    6.5% on the carrier-mixing synth benchmark rows. See [#2805](https://github.com/doppler-dsp/doppler/issues/2805).
