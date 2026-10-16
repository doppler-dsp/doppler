- **A vendored `jm_*.h` can no longer fall behind the pinned just-makeit
    unnoticed.** `make drift-check` now fails when `jm_bench.h`,
    `jm_test.h`, `jm_perf.h` or `jm_simd.h` -- jm's own files, written once
    and never rewritten -- differs from the pinned jm's render, and
    `make jm-revendor` replaces each stale copy through `jm apply`. It found
    two more: `jm_perf.h` lacked jm's C++ `restrict` fix, and the downstream
    example still carried jm 0.60.0's `jm_bench.h` with the silent 32-row
    cap ([#2377](https://github.com/doppler-dsp/doppler/issues/2377)).
