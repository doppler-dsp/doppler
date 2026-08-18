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
