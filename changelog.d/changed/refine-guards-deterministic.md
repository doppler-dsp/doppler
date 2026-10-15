- **Refine's two rate refinements are held by deterministic tests.**
    `test_burst_capture_core.c` places a swept burst in cases where removing
    the rate-widened Doppler cells, or the denser rate grid, names the wrong
    repetition. `validate_acq_rate_pd --check` keeps its seeded capture-Pd
    pin as a broad backstop for smaller regressions (#2219). Test-only.
