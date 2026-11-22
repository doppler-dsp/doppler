- **`make bench-run-check` measures only doppler's own benchmarks.** It
    searched all of `build/`, including the downstream jm example's
    separate build in `build/downstream-jm-py`. That example's
    `c:capture::*` rows then failed the PR label check as unlabelled
    doppler rows on any local run after its tests; CI was unaffected.
    Discovery now stops at any nested `CMakeCache.txt`.
