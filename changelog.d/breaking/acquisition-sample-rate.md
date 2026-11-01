- **`Acquisition` takes the `sample_rate` it is fed at; `spc` is gone.** One
    code period is `sample_rate * len(code) / chip_rate` samples, which must
    be a whole number, and need no longer be a whole number of samples a
    chip: a power-of-two period (2,048 for a 1,023-chip code) runs the
    search's transforms 5x faster than 2 * 1023 = 2,046. Pass
    `sample_rate=spc * chip_rate` for the old behaviour, which is unchanged
    bit for bit. In C, `dp_acq_create_continuous()` takes `sample_rate` in
    place of `spc`, `dp_acq_build_handoff()` drops its `spc` argument, the
    `spc` field is removed, and `dp_acq_period_samples()` derives the
    smooth period.
