- **uv is pinned, so a re-lock no longer rewrites `uv.lock`'s markers.** The
    lock's bytes depended on whichever uv wrote it: v0.65.0's version bump
    changed 308 lines. `pyproject.toml`'s `[tool.uv] required-version`
    (0.12.24) is now the one pin, every CI install reads it, and
    `make lint-uv-pin`/`lint-uv-lock` gate both (#1940). Developers need that
    uv: `uv self update "$(make -s print-uv-version)"`.
