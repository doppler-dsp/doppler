- **The `sweep` validators' spot checks moved out of `make test` into their
    own CI job, cutting the C suite from 332 s to 38 s.** They were 88.5% of
    it (294.2 s of 332.4 s) and `make test` runs on every platform build, so
    that cost was paid three times per push for numeric checks that do not
    vary by platform. Nothing stopped running — `make test-sweep` runs them
    once, on the pinned image. `make test TEST_SWEEP=1` restores the full
    battery, as `SAN_SWEEP=1` and `COV_SWEEP=1` do for their suites.
