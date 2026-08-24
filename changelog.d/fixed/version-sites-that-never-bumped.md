- **Two `[project] version` fields had never been bumped, and nothing was
    watching either.** `bootstrap.toml` sat at `0.3.7`, frozen when `jb.toml`
    was renamed; `just-makeit.toml` sat at `0.1.0`, the value it was given in
    the **initial commit**, unchanged across every release to 0.43.2. Both sit
    beside `name = "doppler"`, so both read as this project's version and both
    were simply wrong. `make version-check` probed three files and neither was
    among them.

    Nothing consumed either, which is exactly why nothing noticed —
    `doppler_version()` returns `DOPPLER_VERSION`, stamped by CMake from
    `PROJECT_VERSION`. But `just-makeit.toml`'s has a future:
    [just-makeit#1141](https://github.com/just-buildit/just-makeit/issues/1141)
    is that `[project] version` reaches none of its four generated copies
    (`pyproject.toml`, `CMakeLists.txt`, `bootstrap.toml`, `<proj>_version()`).
    When jm closes that, this value starts propagating outward, and `0.1.0`
    would have propagated *into* the three files that were right.

    Both are now correct, in `VERSION_PROBES` and in `BUMP_VERSION_CMD` — five
    probed files instead of three. Each new probe was mutation-tested to fail
    on its own, and the bump was run and reverted to confirm it moves all five
    and leaves `jm_version` — a different number with its own SSOT in
    `scripts/gen_jm_pin.py`, two lines away — untouched. Both seds are
    range-scoped to the `[project]` table for that reason.
