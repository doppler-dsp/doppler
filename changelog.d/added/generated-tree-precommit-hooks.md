- **The generated tree is checked at the commit that changes it, not in CI.**
    Two pre-commit hooks close the gap `make lint` structurally cannot see —
    lint checks *sources*, while `docs/c-api` is mkdoxy output and the docstring
    ratchet reads generated stubs.

    `gen-c-api-drift` runs `make gen-c-api-check` when a staged file matches the
    Doxyfile's own `INPUT` and `FILE_PATTERNS`, so editing a header and
    forgetting the regenerated tree fails the commit that caused it. It is
    narrowed rather than unconditional because it costs 19 s and shells out to
    a pinned doxygen container on any box whose doxygen is not 1.9.8.

    `docs-invariants` runs the thirteen fast checks `docs-check` runs before its
    site build — API-doc coverage, the docstring-coverage ratchet, nav index,
    doc/face parity, version strings, generator drift — in **1.6 s**, on every
    commit. It replaces the narrower `docs-drift` hook, whose four checks it
    contains, and iterates `DOCS_CHECK_PRE_CMDS` directly so the hook and the
    gate cannot disagree about what an invariant is.

    Both run the same commands CI runs, so a local pass means a CI pass.
