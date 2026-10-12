- **`rate_hypotheses=0` chooses the count.** The fewest hypotheses whose
    residual `doppler_rate/(n - 1)` no longer limits the depth the sizer
    would pick, read back as `rate_hypotheses`: 4 where 9 was hand-picked for
    a Zadoff-Chu 127 x 16 preamble at +-9e-7 (capture Pd 0.916 against 0.912,
    about 0.45x the per-dwell work), 1 when a strong design point needs no
    compensation. Burst engines only, and it needs a `doppler_rate` to span.
    Measured by `validate_acq_rate_pd`.
