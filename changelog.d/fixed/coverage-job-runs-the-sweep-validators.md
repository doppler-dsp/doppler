- **The coverage job ran the `sweep` validators under instrumentation, and
    was cancelled at its 90-minute cap.** The `validate_*` harnesses register
    a `--check` spot check as a ctest entry, cheap in the Release suite and
    ruinous instrumented: the 34 of them are **94.3% of the instrumented ctest
    CPU** (4615.7 s of 4893.6 s over 20 cores), and `validate_acq_surface_jitter`
    alone takes 1261.7 s against a 1266.0 s leg — the suite finishes when that
    one test finishes, so no amount of `-j` helps. ASan, UBSan and TSan each
    excluded them years ago with their own measurement; coverage never picked
    it up, and the omission had no symptom until the cap began cancelling the
    job, at which point it read as flaky infrastructure. Every open PR in the
    repo was blocked for a day. The leg now passes `$(COV_EXCLUDE_SWEEP)`
    (`COV_SWEEP=1` restores them, as `SAN_SWEEP=1` does) and runs in 55.7 s.
    What the report loses is **nine lines of 29738**, 86.05% → 86.02%, each a
    line a validator reaches and no unit test does.
    [#1292](https://github.com/doppler-dsp/doppler/issues/1292).
