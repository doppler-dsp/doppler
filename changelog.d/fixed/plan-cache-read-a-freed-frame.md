- **A Plan rendered a framed scene with no noise at all, at any SNR.**
    `wfm_plan` kept a *borrowed* `frame` pointer into the composer
    `plan_build()` destroys, and the read-after-free made every noise draw
    fail silently — full sample count, exit 0, clean waveform. Caught by ASan;
    found by `example-projects/burst-pipeline`, whose "28x once prepared" was
    mostly the missing AWGN.
    [#1158](https://github.com/doppler-dsp/doppler/issues/1158)
