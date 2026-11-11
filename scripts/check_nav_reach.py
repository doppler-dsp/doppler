#!/usr/bin/env python3
"""Every page under ``docs/`` is in the BUILT site's nav, or says it is not.

The nav is discovered, not listed (#3000): ``docs/.nav.yml`` sets the top
level and each section directory's own ``.nav.yml`` ends in a glob, so a new
page appears in its section by existing. That holds only while every
section keeps its glob. A section that lists its pages and forgets the glob,
or a new directory no ``.nav.yml`` reaches, builds a page nobody can find
from the nav -- and every other docs gate stays green, because the page
itself is fine. Two gates used to guard this, each over its own list
(``check_nav_index.py``: every page has a bullet in its section's index;
``gen_example_projects.py``: every project page is in ``mkdocs.yml``'s nav
once). This one replaces both by asking the artifact.

A page is reached when its URL is a link in the primary nav of some built
page (each page renders the nav with its own section open, so the union over
the site is the whole tree). A page that is deliberately NOT in the nav is
declared once, in ``mkdocs.yml``'s ``not_in_nav`` -- the setting zensical
itself reads -- so the gate and the build agree on what "deliberately"
means. ``exclude_docs`` pages are not built at all and owe nothing.

Both settings are gitignore patterns. This reads the subset doppler uses
and refuses any other, rather than guess at one it cannot match:

* ``/dir/`` -- everything under ``docs/dir/``;
* ``/dir/*.md``, ``/name.md`` -- an anchored glob (``*`` stays within one
  directory);
* ``**/dir/`` -- a directory of that name at any depth;
* ``!<pattern>`` -- re-include what an earlier pattern excluded.

Stdlib only, so it runs on the floor Python like every other gate. Run
after ``make docs``:

    uv run python scripts/check_nav_reach.py
"""

from __future__ import annotations

import argparse
import fnmatch
import posixpath
import re
import sys
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def setting(mkdocs_yml: str, key: str) -> list[str]:
    """The lines of a ``key: |`` block scalar in ``mkdocs.yml``.

    Raises
    ------
    ValueError
        ``key`` is present but not written as a ``|`` block, the one form
        this reads (a YAML list or a one-line string would be misread).
    """
    m = re.search(rf"^{key}:(.*)$", mkdocs_yml, re.M)
    if not m:
        return []
    if m.group(1).strip() != "|":
        raise ValueError(f"mkdocs.yml: write {key} as a `|` block")
    out = []
    for line in mkdocs_yml[m.end() + 1 :].splitlines():
        if line and not line.startswith(" "):
            break
        if line.strip() and not line.strip().startswith("#"):
            out.append(line.strip())
    return out


def _matches(pattern: str, rel: str) -> bool:
    """Does one gitignore pattern (this script's subset) match ``rel``?

    ``rel`` is a page's path under ``docs/``, POSIX, no leading slash.
    """
    if pattern.startswith("**/") and pattern.endswith("/"):
        name = pattern[3:-1]
        return f"/{name}/" in f"/{rel}"
    if not pattern.startswith("/") or "**" in pattern:
        raise ValueError(
            f"mkdocs.yml: pattern '{pattern}' is outside the subset "
            "check_nav_reach.py reads (see its docstring)"
        )
    body = pattern[1:]
    if body.endswith("/"):
        return rel.startswith(body)
    # `*` stays inside one directory: same depth, then a glob match.
    return rel.count("/") == body.count("/") and fnmatch.fnmatchcase(rel, body)


def excluded(patterns: list[str], rel: str) -> bool:
    """gitignore semantics: the LAST pattern that matches decides."""
    out = False
    for p in patterns:
        neg = p.startswith("!")
        if _matches(p[1:] if neg else p, rel):
            out = not neg
    return out


def url_of(rel: str) -> str:
    """The site path a ``docs/`` page builds to (directory URLs)."""
    stem = rel[: -len(".md")]
    if stem == "index":
        return ""
    if stem.endswith("/index"):
        return stem[: -len("/index")]
    return stem


class _Nav(HTMLParser):
    """The hrefs of the primary nav's links on one built page."""

    def __init__(self) -> None:
        super().__init__()
        self.depth = 0
        self.hrefs: list[str] = []

    def handle_starttag(self, tag: str, attrs: list) -> None:
        a = dict(attrs)
        cls = a.get("class") or ""
        if tag == "nav" and "md-nav--primary" in cls and not self.depth:
            self.depth = 1
            return
        if not self.depth:
            return
        if tag == "nav":
            self.depth += 1
        if tag == "a" and "md-nav__link" in cls and a.get("href"):
            self.hrefs.append(a["href"])

    def handle_endtag(self, tag: str) -> None:
        if self.depth and tag == "nav":
            self.depth -= 1


def reached(site: Path) -> set[str]:
    """Every site path some built page's primary nav links to."""
    out: set[str] = set()
    for page in site.rglob("index.html"):
        here = page.parent.relative_to(site).as_posix()
        here = "" if here == "." else here
        nav = _Nav()
        nav.feed(page.read_text(encoding="utf-8"))
        for href in nav.hrefs:
            href = href.split("#")[0]
            if not href or "://" in href:
                continue
            target = posixpath.normpath(posixpath.join(here, href))
            out.add("" if target == "." else target.strip("/"))
    return out


def unreached(root: Path, site: Path) -> list[str]:
    """Every built ``docs/`` page the nav does not reach and nothing excuses.

    Raises
    ------
    ValueError
        ``exclude_docs`` or ``not_in_nav`` is written in a form this cannot
        read.
    """
    yml = (root / "mkdocs.yml").read_text(encoding="utf-8")
    skip = setting(yml, "exclude_docs")
    quiet = setting(yml, "not_in_nav")
    docs = root / "docs"
    nav = reached(site)
    out = []
    for md in sorted(docs.rglob("*.md")):
        rel = md.relative_to(docs).as_posix()
        if excluded(skip, rel) or excluded(quiet, rel):
            continue
        if url_of(rel) not in nav:
            out.append(rel)
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--site", type=Path, default=None)
    a = ap.parse_args(argv)
    site = a.site or a.root / "site"
    if not (site / "index.html").exists():
        print(
            f"nav-reach: no built site at {site}; run `make docs` first",
            file=sys.stderr,
        )
        return 1
    try:
        missing = unreached(a.root, site)
    except ValueError as e:
        print(f"nav-reach: FAILED\n  {e}", file=sys.stderr)
        return 1
    if missing:
        print("nav-reach: FAILED", file=sys.stderr)
        for rel in missing:
            print(
                f"  docs/{rel}: built, but no page's nav reaches it -- end "
                "its section's .nav.yml in a glob, or list it in mkdocs.yml "
                "not_in_nav",
                file=sys.stderr,
            )
        return 1
    print("nav-reach: OK -- every docs/ page is in the nav or not_in_nav")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
