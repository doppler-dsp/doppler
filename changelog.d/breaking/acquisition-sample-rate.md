- **`Acquisition` takes the `sample_rate` it is fed at; `spc` is gone.** One
    code period is `sample_rate * len(code) / chip_rate` samples, which must
    be a whole number, and need no longer be a whole number of samples a
    chip. Pass `sample_rate=spc * chip_rate` for the old behaviour. In C,
    `dp_acq_create_continuous()` takes `sample_rate` in place of `spc`,
    `dp_acq_build_handoff()` drops its `spc` argument, and the `spc` field
    is removed.
