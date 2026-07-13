#!/usr/bin/env python3
"""Generate README.md's "## Quick start" section from docs/index.md.

docs/index.md and README.md deliberately show the identical Quick Start
walkthrough, but render on two different engines -- mkdocs-material
admonitions (``!!! tip``) on the docs site, GitHub's native alert syntax
(``> [!TIP]``) on GitHub -- so the two pages can never be byte-identical.
Keeping them in sync by hand rotted repeatedly across a single session
(a live tagline edit, a missing `git clone` step, a stale quickstart link)
before this generator existed. docs/index.md is now the single source of
truth: this script extracts its "## Quick start" section, rewrites it for
GitHub rendering, and writes the result into README.md between marker
comments -- same idiom as ``gen_related_pages.py``'s
``<!-- related-pages:start -->``/``:end`` block.

Rewrites applied:
    - ``!!! tip "TITLE"`` followed by a single-line indented fenced
      command becomes GitHub's ``> [!TIP]`` blockquote alert.
    - a relative link to another docs page (``quickstart.md``,
      ``install/c.md#anchor``) is rewritten to ``docs/...`` so it
      resolves from the repo root, where README.md lives -- docs/index.md
      itself lives *inside* docs/, so its relative links are one level
      shallower than README.md needs.

Usage
-----
    python scripts/gen_readme_quickstart.py --write   # regenerate the block
    python scripts/gen_readme_quickstart.py --check   # exit 1 on any drift
"""

from __future__ import annotations

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INDEX_MD = os.path.join(ROOT, "docs", "index.md")
README_MD = os.path.join(ROOT, "README.md")

START_MARKER = "<!-- quickstart:start -->"
END_MARKER = "<!-- quickstart:end -->"

# From the "## Quick start" heading up to (not including) the next
# level-2 heading (e.g. "## Build") -- "### Python"/"### C" subheadings
# don't match ``^## `` so they stay inside the captured section.
SECTION_RE = re.compile(
    r"^## Quick start\n.*?(?=^## )", re.MULTILINE | re.DOTALL
)

# !!! tip "TITLE"
#
#     ```lang
#     single line command
#     ```
TIP_RE = re.compile(r'!!! tip "([^"]+)"\n\n {4}```\w+\n {4}(.+)\n {4}```\n')

# A markdown link whose target isn't a full URL, an in-page anchor, or
# already docs/-prefixed -- e.g. [Quick Start](quickstart.md) or
# [C Library](install/c.md#install-from-a-release-tarball). The
# leading-``!``-exclusion skips image embeds (``![alt](src)``).
LINK_RE = re.compile(r"(?<!!)\[([^\]]+)\]\((?!https?://|#|docs/)([^)]+)\)")


def extract_section() -> str:
    with open(INDEX_MD, encoding="utf-8") as f:
        text = f.read()
    m = SECTION_RE.search(text)
    if not m:
        raise SystemExit(
            "gen_readme_quickstart: could not find a '## Quick start' "
            "section in docs/index.md"
        )
    return m.group(0)


def rewrite_for_readme(section: str) -> str:
    def tip_sub(m: re.Match[str]) -> str:
        title, cmd = m.group(1), m.group(2)
        return f"> [!TIP]\n> {title} `{cmd}`\n"

    rewritten, n = TIP_RE.subn(tip_sub, section)
    if n == 0:
        raise SystemExit(
            "gen_readme_quickstart: found no '!!! tip' admonition to "
            "rewrite in docs/index.md's Quick Start section -- did its "
            "shape change? Update TIP_RE to match the new form."
        )

    def link_sub(m: re.Match[str]) -> str:
        return f"[{m.group(1)}](docs/{m.group(2)})"

    return LINK_RE.sub(link_sub, rewritten)


def render_readme_block(section: str) -> str:
    body = rewrite_for_readme(section).rstrip("\n")
    # mdformat (pre-commit) inserts a blank line between an HTML comment
    # and adjacent content -- emit it ourselves so --write and mdformat
    # agree on the same fixed point instead of fighting each other.
    return f"{START_MARKER}\n\n{body}\n\n{END_MARKER}\n"


def apply(write: bool) -> bool:
    """Regenerate README.md's marked block. Returns True if already
    up to date (no drift)."""
    new_block = render_readme_block(extract_section())

    with open(README_MD, encoding="utf-8") as f:
        readme = f.read()
    pattern = re.compile(
        re.escape(START_MARKER) + r".*?" + re.escape(END_MARKER) + r"\n?",
        re.DOTALL,
    )
    if not pattern.search(readme):
        raise SystemExit(
            f"gen_readme_quickstart: no {START_MARKER} .. {END_MARKER} "
            "block found in README.md -- add the markers around the "
            "Quick Start section once, then re-run."
        )

    # A function replacement, not a string one -- new_block can contain
    # literal backslash sequences (e.g. a C snippet's "\n"), and
    # re.Pattern.sub() interprets backslash escapes in a string
    # replacement (\n, \1, \g<name>, ...), corrupting them silently.
    updated = pattern.sub(lambda _: new_block, readme, count=1)
    if updated == readme:
        return True
    if write:
        with open(README_MD, "w", encoding="utf-8") as f:
            f.write(updated)
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--write", action="store_true", help="regenerate README.md's block"
    )
    group.add_argument(
        "--check",
        action="store_true",
        help="exit 1 on any drift, write nothing",
    )
    args = parser.parse_args()

    up_to_date = apply(write=args.write)
    if not up_to_date:
        if args.check:
            print(
                "README.md's Quick Start section is out of sync with "
                "docs/index.md -- run: "
                "python scripts/gen_readme_quickstart.py --write",
                file=sys.stderr,
            )
            return 1
        print("README.md's Quick Start section regenerated from docs/index.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
