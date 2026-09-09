- **Re-vendored `standard.mk` again.** Upstream filed `gates-home-check`
    under `make help`'s Aggregates section (it had fallen through to Local,
    advertising a shared target as repo-specific) and `help-check` now fails
    on any standard target filed as Local. Comment and help-menu only; the
    vendored copy being behind failed `standard-check` on every branch,
    which is why this is its own change.
