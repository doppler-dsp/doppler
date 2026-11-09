- **Viterbi, BurstDemod, ppe and CarrierAcquisition size their buffers
    through the checked helpers** (`dp_mallocn`, `dp_xmallocn`,
    `dp_xreallocn`), 26 sites. Two wrapping counts are now refused rather
    than sized short: a Viterbi `depth` whose decision buffer
    `depth * nstate` wraps fails `create`, and a `BurstDemod`
    `set_preamble` whose `reps * est_segments` wraps leaves the object as
    it was.
