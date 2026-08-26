- **`DsssBurstReceiver`'s benchmark recorded nothing.** jm's scaffold
    correctly reported that the component has no `step()` to time and then
    stopped, leaving a `main()` that called `jm_bench_write_json` without
    ever calling `jm_bench_add` — so it wrote an empty `"benchmarks": []`
    into every snapshot. That is worse than an absent benchmark, because it
    runs green and reads as "measured, nothing to report".

    `push()` is the benchable shape, and it is deliberately reported as
    **two** numbers rather than one, because its stages do not run equally
    often: search runs on every sample and sets the sustained rate a caller
    can feed, while refine and demod run once per detection. `push_idle`
    (noise only, nothing decodes) is the price of listening; `push_burst`
    (one full burst per block, decoded end to end) is that plus what a
    detection costs. A single blended figure would hide which of the two a
    caller is paying for. Both print their decoded-burst count, so a run
    that quietly stopped detecting shows up as suspicious rather than fast.
