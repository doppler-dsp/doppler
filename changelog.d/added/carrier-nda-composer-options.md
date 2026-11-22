- **`CarrierNda` takes three options for a loop updated once a block**, each
    off by default, so `steps()` and every caller are unchanged:
    `set_update_period()` (bn per update, the command spread over it),
    `set_denominator(alpha, gamma_d)` (the error over an average of `|z|^2`,
    or that less a noise power, clamped; M = 2) and `set_block_lock()` (the
    ratio of sums `(I^2 - Q^2)/(I^2 + Q^2)` on a block against its exact
    Beta threshold; M = 2). A fifth probe, `.nco`, is the full NCO command
    ([async-dsss-receiver](docs/design/async-dsss-receiver.md)).
