#!/usr/bin/env python3
"""Generate CHANGELOG.md's comparison-link block from its own headings.

A `## [0.44.0] — 2026-08-24` heading is a markdown REFERENCE link. Without a
matching `[0.44.0]: https://...` definition at the foot of the file it renders
as the literal text `[0.44.0]` — on the docs site and on GitHub both.

`docs/dev/release.md` step 4 asked the releaser to add that line by hand, and
nothing checked. The cost was not hypothetical: 0.43.0, 0.43.1 and 0.43.2 all
shipped without one and there was no `[unreleased]:` definition at all
(doppler#996). Backfilling those four was not a fix — the next release drops
one again — and on this script's first run it found **seven more** nobody had
noticed: 0.4.6, 0.20.0, 0.21.0, 0.29.0, 0.30.0, 0.31.0 and 0.32.0.

So this GENERATES rather than checks. A checker would have turned one manual
step into one manual step plus a red gate; the release step disappears
instead. That is the difference between policing friction and removing it.

The links are DERIVED, so there is nothing to keep in sync: each version
compares against the next-older heading in the file, the oldest points at its
release tag (it has no predecessor), and `[unreleased]` compares the newest
against HEAD. Adding a release section is the whole of the work.

Usage
-----
    python scripts/gen_changelog_links.py --check   # exit 1 on drift
    python scripts/gen_changelog_links.py --write   # rewrite the block
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CHANGELOG = ROOT / "CHANGELOG.md"
REPO = "https://github.com/doppler-dsp/doppler"

HEADING_RE = re.compile(r"^## \[([^\]]+)\]", re.M)
DEF_RE = re.compile(r"^\[[^\]]+\]: \S+$", re.M)


def render(headings: list[str]) -> list[str]:
    """The link block for `headings`, newest-first as they appear in the file.

    Sorted the way the file already sorted them -- lexicographically, with
    `[unreleased]` last. That is not a semver order (0.10.0 sorts before
    0.9.0) and it does not need to be: nothing reads this block in order, and
    matching what is there keeps the first diff to the seven genuinely
    missing lines rather than a reshuffle of eighty-one.
    """
    versions = [h for h in headings if h.lower() != "unreleased"]
    out: dict[str, str] = {}
    for i, v in enumerate(versions):
        if i + 1 < len(versions):
            prev = versions[i + 1]
            out[v] = f"[{v}]: {REPO}/compare/v{prev}...v{v}"
        else:
            # The oldest release has no predecessor to compare against.
            out[v] = f"[{v}]: {REPO}/releases/tag/v{v}"
    lines = [out[v] for v in sorted(out)]
    if versions:
        lines.append(f"[unreleased]: {REPO}/compare/v{versions[0]}...HEAD")
    return lines


def current_block(text: str) -> tuple[int, int, list[str]]:
    """Span and content of the trailing run of link definitions."""
    matches = list(DEF_RE.finditer(text))
    if not matches:
        return len(text), len(text), []
    # Only the CONTIGUOUS trailing run is the block; a stray definition in
    # prose higher up is not ours to move.
    end = matches[-1].end()
    run = [matches[-1]]
    for m in reversed(matches[:-1]):
        between = text[m.end() : run[-1].start()]
        if between.strip():
            break
        run.append(m)
    start = run[-1].start()
    return start, end, [m.group(0) for m in reversed(run)]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--check", action="store_true")
    g.add_argument("--write", action="store_true")
    a = ap.parse_args()

    text = CHANGELOG.read_text(encoding="utf-8")
    headings = HEADING_RE.findall(text)
    if not headings:
        print(
            "gen_changelog_links: FAIL -- no `## [version]` headings in\n"
            "  CHANGELOG.md. Nothing to derive from means nothing was\n"
            "  derived; refusing to write an empty block.",
            file=sys.stderr,
        )
        return 1

    want = render(headings)
    start, end, have = current_block(text)

    if have == want:
        print(
            f"Changelog links: OK — {len(want)} definition(s) for "
            f"{len(headings)} heading(s)"
        )
        return 0

    if a.write:
        CHANGELOG.write_text(
            text[:start] + "\n".join(want) + text[end:], encoding="utf-8"
        )
        print(f"Changelog links: regenerated — {len(want)} definition(s)")
        return 0

    missing = [w for w in want if w not in have]
    stale = [h for h in have if h not in want]
    print("Changelog links drift — run `make docs-relink`:", file=sys.stderr)
    for m in missing:
        print(f"  + {m}", file=sys.stderr)
    for s in stale:
        print(f"  - {s}", file=sys.stderr)
    print(
        "\n  A `## [X.Y.Z]` heading with no definition renders as the\n"
        "  literal text `[X.Y.Z]` on the docs site and on GitHub.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
