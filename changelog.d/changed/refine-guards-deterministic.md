- **Refine's two rate refinements are held by deterministic tests, not a
    seeded Pd pin.** `test_burst_capture_core.c` places a swept burst in
    cases where removing the rate-widened Doppler cells, or the denser rate
    grid, names the wrong repetition; `validate_acq_rate_pd --check` no
    longer pins its seeded capture Pd for them (#2219). Test-only.
