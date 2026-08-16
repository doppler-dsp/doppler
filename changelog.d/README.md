# `changelog.d/` — one file per entry, so entries cannot collide

Every open pull request used to append to the same place in `CHANGELOG.md`.
Measured on 2026-08-16 with twelve PRs in flight: **all twelve** touched the
file, all of them near the top of a 2625-line `[Unreleased]`, so **each merge
knocked the other eleven to `CONFLICTING`**. That is `O(N^2)` hand-resolutions
in one file, none of them about the code — and it is a property of the
*layout*, not of anyone's discipline.

So an entry is now a **file**. Two PRs touch different files; git has nothing
to resolve.

## Writing one

Drop a markdown file in the directory named for its section:

```
changelog.d/<section>/<slug>.md
```

`<section>` is one of `added`, `changed`, `fixed`, `removed`, `breaking`,
`deprecated`, `security` — the directory IS the `### Heading` it lands under,
so nothing has to be declared twice. `<slug>` is yours; make it describe the
change (`mf-in-noise-bandwidth.md`), because it is what a reviewer sees in the
file list.

The content is the entry exactly as it will appear, starting with `- `:

```markdown
- **The headline, in bold.** Then the prose, at the same standard the rest of
    this changelog is written to — what changed, what it was measured
    against, and what it cost. Continuation lines are indented four spaces.
```

Nothing is generated from it and nothing is templated. It is moved verbatim.

## Releasing

`make changelog-assemble` promotes every fragment into `## [Unreleased]` under
its section heading and deletes the fragments, in one commit. That is the only
time `CHANGELOG.md` is edited by hand-written content, and it happens once per
release instead of once per PR.

## The gate

`make changelog-check` is unchanged in what it *asks* — a branch that changes
code must say what changed — and now accepts either a fragment or a direct
`CHANGELOG.md` edit. It counts fragments toward `[Unreleased]` being non-empty,
so a release is never cut with the notes still sitting here unassembled.

Editing `CHANGELOG.md` directly still works and is still right for a release
commit. For a normal change, write a fragment: it is the same words in a file
that nobody else is touching.
