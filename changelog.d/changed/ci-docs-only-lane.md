- **A docs-only PR skips every job docs cannot break.** standard.mk's `make ci-docs`
    answers `code=false` when every changed path matches `CI_DOCS_RE` and
    nothing outside `docs/` or `changelog.d/` was deleted. Then the C
    builds, Doxygen, sanitizers, coverage, glibc, packages, Docker and the
    sweep skip (`CI passed`'s `CODE_ONLY`). Lint, the site build and the
    full Python suite on one leg still run. `make ci-aggregator-check`
    holds `CODE_ONLY` to the jobs gated on `code`.
