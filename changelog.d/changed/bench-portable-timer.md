- **Benchmarks time through one clock.** All 106 benchmarks now call
    `jm_bench_now_ns()` / `jm_bench_elapsed_sec()` from the vendored
    `jm_bench.h` instead of opening `clock_gettime(CLOCK_MONOTONIC)` each and
    carrying 85 private copies of a four-line `elapsed_sec()`. The UCRT has
    neither, so this is also what lets them compile on Windows. Published
    numbers are unaffected — the two formulas agree to 8.2e-11 relative,
    against ~1e-2 run-to-run noise. Gated by `make lint-bench-timer`:
    [#1368](https://github.com/doppler-dsp/doppler/pull/1368).
