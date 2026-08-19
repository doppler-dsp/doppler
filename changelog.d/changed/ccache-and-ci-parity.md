- **The C core is compiled through ccache, and a developer can now run the
    gates in CI's own container.** Two halves of the same idea: stop repeating
    work, and stop guessing whether the environment matches.

    Measured first. On one run the core was built **seven times with the same
    compiler and the same flags** — once per Python job (only the ~11 s
    extension build differs per ABI) plus the ubuntu-24.04 leg — at 59–80 s
    each, about 7 minutes of identical work per run. `ccache` is now in every
    dev group and reaches every configure step, including the coverage tree's.

    What is *not* duplicated stays that way: the ubuntu-22.04 leg is a
    different glibc and compiler, the coverage build is clang with
    `-fprofile-instr-generate`, and `glibc-228` is the floor's toolchain.
    ccache hashes the compiler and the full flag set, so those land in
    separate entries by construction rather than by our being careful about
    it. `make ccache-stats` runs after each build so the hit rate is in the
    log — a cache that stops hitting has no other symptom than builds slowly
    getting longer.

    ccache rather than build-once-and-share: an artifact would make the six
    Python jobs *wait* on a build job that nothing waits on today, and it
    cannot span runs. The cache can, which is the case a developer feels — the
    second push to a branch.

    **Running it like CI is now a target, not a hope.** `make ci-run   TARGET='build test-rust'` runs any goals inside the *pinned digest* CI
    uses, `make ci-shell` opens a shell in it, and `make ci-gates` runs the
    whole gate set there — `gates` is already "every gate CI runs" (enforced
    by `gates-check`), so this only adds the environment.

    Both of today's CI-only failures would have been caught by it before a
    push: a cargo too old to read the repo's own lockfile (invisible on a box
    with rustup), and a host build tree handed to the container failing on
    `atan2f@GLIBC_2.43`. That second one is why `ci-run` builds into its own
    `build-ci/` and sets **both** `BUILD_DIR` and `DOPPLER_BUILD_DIR` —
    `ffi/rust/build.rs` locates the library itself and defaults to the host
    tree, so missing the second variable fails in a way that reads as a code
    bug. Same separation, same reason, as `glibc-gate`'s `build-glibc228`.

    Deliberately not a pre-push hook: `gates` includes `coverage` at ~10
    minutes, and a hook that slow is one people pass `--no-verify` to.
