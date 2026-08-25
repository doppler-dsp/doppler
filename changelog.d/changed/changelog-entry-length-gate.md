- **A changelog entry over 10 lines fails `make lint`.** The rule that an
    entry is an index, not the record, was guidance in
    `changelog.d/README.md` — and guidance is what produced a 72,636-character
    v0.44.0 section at a median of 24 lines per entry. `MAX_ENTRY_LINES` is
    checked on the fragments *and* on `[Unreleased]`, so a direct CHANGELOG
    edit cannot walk past it. Folded into `check_release_notes_size.py` rather
    than added beside it: same file, same subject, one gate.

- **`release-notes-size-check` runs in CI now.** It was in `GATES_DEPS` and
    nowhere else, and no CI job runs `make gates` — the exact trap the
    `lint:` comment above it already described, still live for this one gate
    a release after `changelog-check` was moved for the same reason.
