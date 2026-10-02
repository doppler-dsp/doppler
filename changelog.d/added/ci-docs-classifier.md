- **`make ci-docs` says whether a diff is docs-only.** It prints
    `docs=` (did any path match `CI_DOCS_RE`, the Makefile's one
    declaration of what the docs are) and `code=` (did anything else
    change). A deletion outside `docs/` or `changelog.d/` is never
    docs-only, and any doubt answers `code=true`. CI's docs-only lane
    gates on it.
