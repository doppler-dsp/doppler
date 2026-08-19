- **Timing the Python microbenchmarks is `make bench-python` now, not part of
    `make test-python`.** Tests run constantly; benchmarks run occasionally.
    They were one target doing both — a second, *serial* pass over
    `src/doppler/*/benchmarks/` so pytest-benchmark would measure instead of
    disabling itself under xdist.

    Measured in CI before changing anything: that pass was **139 s of a 268 s
    step, on all six Python versions** — roughly 14 minutes a run spent timing
    code on a shared runner and throwing the numbers away. Runner timings are
    not trustworthy in the first place; that is what
    [#543](https://github.com/doppler-dsp/doppler/issues/543) deleted
    `perf-regression.yml` over, and why `make bench-interleaved` exists.

    The benchmark files still run in `make test-python` — as **tests**, with
    `--benchmark-disable`, so a broken benchmark script still fails where it
    should. Nothing stopped being asserted: the suite went from 2730 tests
    plus a separate 138 to **2868 in one pass**, 66 s locally.

    `--benchmark-disable` is explicit rather than relied upon. pytest-benchmark
    *also* self-disables whenever xdist is active, which would leave the test
    run's behaviour depending on `-n auto` — and `PYTEST_ARGS="-n 0"` is a
    documented override, under which timing would quietly switch back on inside
    the step everyone runs. (`-p no:benchmark` is the wrong knob and was
    measured as such: it removes the fixture and every benchmark test errors
    with "fixture 'benchmark' not found".)

    Python line coverage now runs on **3.12 only** — the version whose
    `coverage.xml` is uploaded. The other five computed it and discarded it,
    which is just a slower test run; the number the patch gate reads has always
    come from the `coverage` job's single instrumented C ∪ Python ∪ Rust build.
