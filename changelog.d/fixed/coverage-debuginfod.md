- `make coverage` no longer hangs on any machine where `DEBUGINFOD_URLS` is
    set — which on Ubuntu is every machine, because
    `/etc/profile.d/debuginfod.sh` exports it into every shell. `llvm-cov`
    consulted debuginfod for each of the 33 objects it opens, three times over,
    looking for debug info that cannot exist: the objects were built locally
    minutes earlier and carry their own coverage mapping. Every lookup was a
    network round trip ending in a timeout. Measured: one extension `.so` took
    **over 120 s and produced no output**, against **0.105 s** with the
    variable cleared; the full report/show/export trio over all 33 objects now
    runs in **0.275 s**. The coverage recipe clears the variable for the LLVM
    tools only, so a developer's own debuginfod setup is untouched everywhere
    else.
- `make coverage` is roughly twice as fast, from a clean tree: the coverage
    build now keeps Debug's assertions and drops Debug's `-O0`
    (`COV_CFLAGS ?= -g -O2`). The target runs the whole suite twice, and at
    `-O0` that was dominated by the Monte-Carlo validators —
    `validate_rx_battery` alone took 270 s of a 353 s `ctest`, against 28 s in
    the release build. Measured: **ctest 353 s → 77 s, the whole target 10m47
    (incremental) → 4m52 (clean)**, with the report unmoved at 86.40 % region /
    88.85 % line. `RelWithDebInfo` reaches the same speed and was rejected — it
    defines `NDEBUG` and would compile out every `assert`, so the measured tree
    would stop running code the tested tree runs.
