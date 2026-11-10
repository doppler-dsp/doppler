- **Benchmarks pin to the fast cores on kernels that report one max clock
    for every core** (#2987). Debian's 6.12 gives every CPU of a Ryzen AI 9
    465 the same `cpuinfo_max_freq`, so `bench-ab` and `bench-interleaved`
    ran unpinned there, bouncing between Zen 5 and Zen 5c cores and reading
    1.2-1.4x on unchanged code. The fast class now comes from
    `amd_pstate_prefcore_ranking` when the clock does not split the CPUs.
