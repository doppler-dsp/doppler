- **A vendored `jm_*.h` can no longer fall behind the pinned just-makeit
    unnoticed.** doppler keeps jm's C harness headers (`jm_bench.h`,
    `jm_test.h`, `jm_perf.h`, `jm_simd.h`) verbatim. `make drift-check`
    now fails when one differs from the pinned jm's render, and
    `make jm-revendor` replaces each stale copy through `jm apply`. It
    re-vendored three: the downstream example's `jm_bench.h`, vendored at
    the 0.59.0 pin and still carrying the silent 32-row cap; its
    `jm_test.h`; and `jm_perf.h`, which lacked jm's C++ `restrict` fix
    ([#2377](https://github.com/doppler-dsp/doppler/issues/2377)).
