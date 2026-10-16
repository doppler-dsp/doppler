- **A vendored `jm_*.h` can no longer fall behind the pinned just-makeit
    unnoticed.** doppler keeps the jm C harness headers it commits
    (`jm_bench.h`, `jm_perf.h` and `jm_simd.h`, plus the downstream
    example's `jm_test.h`) verbatim. `make drift-check` now fails when one
    differs from the pinned jm's render; the root's `jm_test.h`, which
    doppler declines and git ignores, is not checked. `make jm-revendor`
    replaces each stale copy through `jm apply`, and refuses one with
    uncommitted changes. It
    re-vendored three: the downstream example's `jm_bench.h`, vendored at
    the 0.59.0 pin and still carrying the silent 32-row cap; its
    `jm_test.h`; and `jm_perf.h`, which lacked jm's C++ `restrict` fix
    ([#2377](https://github.com/doppler-dsp/doppler/issues/2377)).
