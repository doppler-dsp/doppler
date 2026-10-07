- **just-makeit pin 0.99.0 → 0.100.0.** It carries a refusal value for a
    `variable_output` kernel whose count can legitimately be 0 (gh-2012):
    `count_type = "int64_t"` with `error_negative`, or `error_sentinel`. Two
    methods use it, `Resampler.execute_ctrl` and
    `DopplerChannel.execute_profile` (see the Breaking entry). `jm apply`
    changed nothing else in this tree.
