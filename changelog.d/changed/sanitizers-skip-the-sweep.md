- **The sanitizer suites no longer re-run the validation spot checks.** The
    `validate_*` harnesses already run their `--check` subset on every PR in
    the ordinary C suite, where it is cheap; under instrumentation the same
    subset was 80% of ASan's ctest time, 80% of UBSan's and 90% of TSan's —
    1166s per run for work already done. Measured locally, UBSan's ctest goes
    156.19s → 34.67s. `make test-asan SAN_SWEEP=1` puts them back.
