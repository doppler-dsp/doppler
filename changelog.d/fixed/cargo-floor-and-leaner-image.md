- **The Rust lockfile went back to a format the distro cargo can read, and the
    CI image lost 860 MB.** One defect, two symptoms
    ([#887](https://github.com/doppler-dsp/doppler/issues/887)).

    `ffi/rust/Cargo.lock` had drifted to format **v4** — written by whichever
    modern cargo last resolved anything — and cargo refuses v4 below 1.78,
    while apt ships **1.75 on both Ubuntu LTSes**. So anyone who provisioned
    from `bootstrap.toml` and ran `make test-rust` got

    ```text
    error: failed to parse lock file at: ffi/rust/Cargo.lock
    Caused by:
      lock file version 4 requires `-Znext-lockfile-bump`
    ```

    CI never saw it: the hosted runner carries a rustup cargo that shadowed
    apt's. Moving CI into a container removed the shadow and the failure
    surfaced on both Ubuntu legs at once, while macOS stayed green.

    **Nothing needed v4.** The crate is edition 2021 with two dependencies,
    and cargo 1.75 compiles the whole tree in under four seconds — measured
    before choosing the fix, because the alternative (provision rustup
    everywhere) is a much larger change to justify on a guess. The lockfile is
    v3 again by a one-line change that touches no dependency version, and
    `Cargo.toml` now declares `rust-version = "1.75"` so the floor is stated
    rather than implied.

    `make cargo-floor-check` holds it. Cargo rewrites the lockfile to v4 the
    first time a modern one resolves anything, silently, and a lockfile is not
    a file anyone reads — so the bump is invisible and the failure lands far
    from its cause. Both halves are why it is a gate rather than a note.

    The image benefits twice over: the rustup toolchain it had grown to work
    around this — **613 MB, the largest single thing in it** — is gone, taking
    `deploy/docker/Dockerfile.ci` from **3.17 GB to 2.31 GB**. What remains is
    what CI genuinely uses: llvm/clang for coverage, the distro Rust for
    `make test-rust`. `deploy/docker/README.md` now carries `doppler-ci`
    beside `doppler-glibc228` — the two images that bake nothing in — with the
    size breakdown and why it is one image rather than one per job shape.
