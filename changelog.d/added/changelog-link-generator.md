- **The CHANGELOG's comparison links are generated, not hand-written.**
    `## [X.Y.Z]` is a markdown reference link; with no matching definition it
    renders as literal text. The release runbook asked for that line by hand
    and nothing checked, so three releases shipped without one and there was
    no `[unreleased]:` definition at all. `scripts/gen_changelog_links.py`
    derives the block from the headings — `make docs-relink` writes it,
    `make lint` fails on drift. It found **eight** more defects on its first
    run: seven missing definitions, and a link chaining `v0.4.1...v0.5.0`
    straight past 0.4.6.
    [#996](https://github.com/doppler-dsp/doppler/issues/996)
