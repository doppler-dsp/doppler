- **just-makeit pin 0.67.3 → 0.68.0.** Pure tooling: `jm apply` produced
    **zero codegen drift** (42644 manifest-owned files matching before and
    after; only the four pin sites moved), and `jm status --check` is clean on
    both the main tree and the downstream example.

    The bump is worth recording anyway, because one of its two new gates
    checks something this release had just finished fixing by hand.

    **gh-1141 — `jm status` reports generated copies of `[project] version`
    that disagree with it.** Six artefacts carry the project's version and a
    bump reached none of the five create-only ones, `--check` clean throughout:
    `pyproject.toml`, the top `CMakeLists.txt`, `bootstrap.toml`, the
    `Doxyfile`'s `PROJECT_NUMBER`, and `<pkg>_version()` in
    `native/src/<pkg>_lib.c`. That last one is a **C API** — a consumer links
    the library, asks what version it is, and is told the version the project
    had on the day it was scaffolded.

    doppler passes it, and only because `just-makeit.toml`'s `[project]   version` was corrected from `0.1.0` — the value it had carried since the
    **initial commit** — earlier in this same release. Verified rather than
    assumed: putting `0.1.0` back makes `make drift-check` fail with
    `VERSION (2) — generated copies of [project] version disagree with it` and
    `2 version-drift (!)`. Without that fix this bump would have landed red,
    which is a fair description of how close the two were.

    doppler is not exposed on the row with teeth: `doppler_version()` returns
    `DOPPLER_VERSION`, stamped by CMake from `PROJECT_VERSION`, rather than a
    scaffold-day literal.

    **gh-1142 — `jm status` reports an orphaned `<comp>_procglobal.h`**, the
    converse of the gh-1140 fix adopted one release earlier: a component that
    *stops* declaring `process_global` leaves a `DO NOT EDIT` header describing
    a rendezvous the same `apply` just stripped out. Nothing orphaned here;
    `dp_interrupt_guard` still declares it.
