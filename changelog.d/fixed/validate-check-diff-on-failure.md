- **`validate.py --check` says what differs, not just that something does.**
    It printed `STALE — <file>` and stopped, which answers none of the first
    questions: which lines, and is this a real edit or a number's last digit? It
    now prints the unified diff (first 40 changed lines, no context, so every
    line shown is a changed one) and states explicitly that on a machine other
    than the one that generated the report, `make validate` will move the
    problem rather than fix it.

    Written because that distinction turned out to matter. Wiring
    `make validate-check` into CI for
    [#816](https://github.com/doppler-dsp/doppler/issues/816) — it sat in
    `GATES_DEPS` with no CI home, so report staleness was checked on developer
    machines and nowhere else — reported **four of eleven reports STALE on the
    runner** while all eleven were up to date locally on the same architecture,
    Python and numpy. Two of the four render their limits exactly as they always
    have, so their drift predates that change: **the committed reports have
    never been reproducible on a machine other than the one that generated
    them.**

    The CI step is therefore *not* wired, and `ci.yml` carries the evidence as a
    comment where the next reader will look. A permanently red job is worse than
    the hole it closes. Diagnosis and options are
    [#820](https://github.com/doppler-dsp/doppler/issues/820); #816 stays open
    with a named blocker instead of an open question.
