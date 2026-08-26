- **`DsssBurstReceiver`'s benchmark recorded nothing.** jm's scaffold has no
    `step()` to time and stopped there, leaving a `main()` that called
    `jm_bench_write_json` without ever calling `jm_bench_add` — an empty
    `"benchmarks": []` in every snapshot, which is worse than an absent
    benchmark because it runs green and reads as "measured, nothing to
    report". `push()` is now measured as **two** numbers, because its stages
    do not run equally often: `push_idle` 30.3 MSa/s is the price of listening
    (search runs per sample), `push_burst` 25.5 MSa/s adds one decoded burst
    per 8192 samples.
