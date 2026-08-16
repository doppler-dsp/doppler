- **A changelog entry is a FILE now, because a shared line does not scale.**
    Every pull request appended to the same place in `CHANGELOG.md`. Measured
    with twelve PRs in flight: **all twelve** touched it, all near the top of a
    2625-line `[Unreleased]`, so **every merge knocked the other eleven to
    `CONFLICTING`** — `O(N^2)` hand-resolutions in one file, none of them about
    the code. That is a property of the layout, not of anyone's discipline, and
    no amount of care fixes it.

    An entry now goes in `changelog.d/<section>/<slug>.md`. The directory **is**
    the `### Heading`, so the section is never declared twice and cannot
    disagree with itself; the content is the entry verbatim, moved and not
    templated. Two PRs touch different files, so git has nothing to resolve.

    **`changelog-check` is folded, not replaced** — it asks exactly what it
    asked before (a branch changing code must say what changed) and now accepts
    either a fragment or a direct `CHANGELOG.md` edit, counting fragments
    toward `[Unreleased]` being non-empty so a release cannot be cut with the
    notes still sitting unassembled. `make changelog-assemble` promotes them
    once per release, in Keep a Changelog order, and deletes them as it goes,
    so a second run is a no-op by construction.

    One consumer had to learn about them: `gen_jm_pin.py` harvests the jm pin
    from the changelog TEXT, so a bump recording its pin in a fragment would
    have been reported as a pin nothing documents. It reads
    `changelog.d/*/*.md` too now.
