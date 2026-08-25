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
- **The headline, in bold.** One or two sentences of why it matters, and the
    one number or symptom that makes it concrete. Continuation lines are
    indented four spaces.
```

Nothing is generated from it and nothing is templated. It is moved verbatim.

## Keep it to a few lines

**A changelog entry is an index, not the record.** Aim for a bold headline
plus one to three lines. If there is more to say — the measurement, the
sabotage that proved the gate, the three hypotheses that were wrong — it goes
in the issue, the PR, or a design page, and the entry **links** there. Those
places keep it next to the code and the discussion; the changelog only has to
get a reader to them.

This is a correction, and the size of it is the argument. The guidance here
used to ask for "what changed, what it was measured against, and what it
cost", at "the same standard the rest of this changelog is written to". That
produced entries with a median of **24 lines**, and a v0.44.0 section of
**1221 lines / 72,636 characters** — 73% of the GitHub release-body budget,
for one release. Rewritten to this standard it is **225 lines / 12,534
characters**, with nothing dropped: what left the file moved into links.

Two practical consequences:

- The release body is `make release-notes VERSION=x.y.z`, which embeds this
    section verbatim. A section that grows without bound eventually cannot be
    published at all.
- An entry nobody finishes reading documents nothing. The detail was not
    wasted — it was in the wrong file.

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
