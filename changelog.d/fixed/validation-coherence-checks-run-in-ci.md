- **The validation reports' coherence checks ran in no CI workflow, on any
    report.** `Report._self_check` refuses a render that contradicts itself — a
    `§N.M` pointing at a section the report does not have, a gap in section 2's
    numbering, a table cell truncated mid-reference, and now limits counted but
    never rendered. It runs from `render()`, `render()` runs from `emit()`, and
    a `write=False` build skips it — so the only paths that reached it were
    `make validate` and `make validate-check`, **and neither is in any CI
    workflow** (`validate-check` sits in `GATES_DEPS`, and no job runs
    `make gates`; grep the workflows for "validate" and the one hit is a
    release-time wheel check).

    So the checks were tested in CI — `test_validation_report.py` drives them
    over seeded reports — and never *applied* in CI to the eleven reports they
    exist for. That is the campaign's founding bug one layer out: not a claim
    nobody executes, but a checker nobody points at the artifact.

    Each module's `test_validation_limits.py` now renders the report it already
    built (`assert_renders`), so all four coherence families are enforced on
    every real report inside `make test-python`. It is free — the report is
    already in memory — and registration-free per object, since a new object
    joins its module's existing `OBJECTS` map. Proven by sabotage: a dangling
    `§9.9` in `ema`'s validator takes exactly
    `test_the_report_renders_coherently[ema]` red and nothing else.

    Staleness is the half this cannot fix — whether the *committed* bytes match
    the generator is still `make validate-check`'s question, and still not in
    CI. Filed as [#816](https://github.com/doppler-dsp/doppler/issues/816),
    which also proposes making `gates-check` bidirectional so the next gate
    added to `GATES_DEPS` without a CI home fails loudly.
