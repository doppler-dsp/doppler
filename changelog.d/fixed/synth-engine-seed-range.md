- **`_SynthEngine` raises `OverflowError` for a `seed` outside `uint32_t`.**
    The hand-owned constructor parsed `seed` through `k`, which masks, so
    `seed=2**32 + 5` built a synth seeded 5 and `seed=-1` one seeded
    `4294967295`. Both are refused now, as the composer-generated `Synth`
    does (#2276).
