- **The accumulators' `madd`, `add2d` and `madd2d` benchmarks measure the
    kernels.** They timed `(NULL, 0)` calls, so the published number was
    call overhead, and code alignment moved it 20%. Each row is now one
    call over 64k samples that checks its own result. Time per call scales
    with n at a steady ~0.75 ns/sample on aarch64 (#1366).
