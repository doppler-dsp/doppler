- **A vendored `jm_*.h` can no longer fall behind the pinned just-makeit
    unnoticed.** `make drift-check` fails when a committed jm harness
    header differs from the pinned jm's render (an untracked one, like the
    root's declined `jm_test.h`, is not checked), and `make jm-revendor`
    replaces each stale copy. It re-vendored three, among them the
    downstream example's `jm_bench.h` with its silent 32-row cap
    ([#2377](https://github.com/doppler-dsp/doppler/issues/2377)).
