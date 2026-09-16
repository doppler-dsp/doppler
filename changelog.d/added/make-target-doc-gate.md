- **A documented `make` goal is now checked against the real targets**
    ([#1348](https://github.com/doppler-dsp/doppler/issues/1348)). The shell
    fence gate collected every docs page but validated only `doppler` lines,
    so a `make` line was inspected by a check that was a no-op on it. It is
    the dual of `help-check` (*target ⇒ documented*; this is *documented ⇒
    target*) and reads `make help`, not a second list. It caught one on its
    first run: `tests/install/rust-test.sh` — included into the Rust install
    page — ran `make rust-test`, which is not a target, and nothing executes
    that script, so the line had never run.
