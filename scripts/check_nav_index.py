#!/usr/bin/env python3
"""Fail when a docs page has no bullet in its section's ``index.md``.

``docs/design/index.md`` and ``docs/dev/index.md`` are hand-curated bullet
lists mapping topic -> page. Nothing enforces they stay in sync with the
pages that actually exist, and they have silently rotted twice already
(caught by a manual survey, not by CI) -- most recently within days of a
brand-new design doc (``timing_lock_detector.md``) landing without a bullet.
This script is the backstop: for each section it unions the on-disk ``*.md``
files with ``mkdocs.yml``'s nav entries for that section (a page can drift
out of either one independently) and checks every page is linked from the
section's ``index.md``.

It works by regex/text scanning -- no markdown parser dependency, matching
the light-touch style of ``check_api_docs.py``.

Usage
-----
    python scripts/check_nav_index.py          # report + exit 1 if any gaps
"""

from __future__ import annotations

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MKDOCS_YML = os.path.join(ROOT, "mkdocs.yml")

# Section name -> docs subdirectory (relative to ROOT). Excludes ``archive/``
# subdirectories, which hold deliberately superseded pages.
#
# ``guide`` is NESTED: it has its own top-level pages plus subsections that
# carry their own ``index.md`` (``guide/wfmgen/``, ``guide/wfm-io/``). A page
# there is listed by the index NEAREST it, not by the section root -- nobody
# adds a bullet for ``wfmgen/coding.md`` to ``guide/index.md``, and asking for
# one would be asking for the wrong thing. So containment resolves to the
# nearest enclosing ``index.md``; for design/dev/gallery, which have none
# below their root, that is exactly the old behaviour.
SECTIONS = {
    "design": "docs/design",
    "dev": "docs/dev",
    "gallery": "docs/gallery",
    "guide": "docs/guide",
}

NAV_ENTRY_RE = re.compile(r":\s*([\w./-]+\.md)\s*$")
LINK_TARGET_RE = re.compile(r"\]\(([^)#]+\.md)(?:#[^)]*)?\)")


def _on_disk_pages(section_dir: str) -> set[str]:
    """Every ``*.md`` under ``section_dir``, relative to it, excluding
    ``index.md`` and anything under an ``archive/`` subdirectory."""
    pages: set[str] = set()
    for dirpath, dirnames, filenames in os.walk(section_dir):
        dirnames[:] = [d for d in dirnames if d != "archive"]
        for fn in filenames:
            if not fn.endswith(".md"):
                continue
            rel = os.path.relpath(os.path.join(dirpath, fn), section_dir)
            rel = rel.replace(os.sep, "/")
            if rel == "index.md":
                continue
            pages.add(rel)
    return pages


def _nav_pages(prefix: str) -> set[str]:
    """Every ``<prefix>/....md`` entry in ``mkdocs.yml``'s nav, relative to
    the section (i.e. with ``<prefix>/`` stripped), excluding ``index.md``."""
    pages: set[str] = set()
    with open(MKDOCS_YML, encoding="utf-8") as fh:
        for line in fh:
            m = NAV_ENTRY_RE.search(line.strip())
            if not m:
                continue
            target = m.group(1)
            if not target.startswith(f"{prefix}/"):
                continue
            rel = target[len(prefix) + 1 :]
            if rel == "index.md":
                continue
            pages.add(rel)
    return pages


def _linked_pages(index_md: str) -> set[str]:
    """Every ``*.md`` link target that appears anywhere in ``index_md``,
    normalized to a bare filename/relative-path (strips a leading ``./``)."""
    with open(index_md, encoding="utf-8") as fh:
        text = fh.read()
    targets = set()
    for m in LINK_TARGET_RE.finditer(text):
        t = m.group(1)
        if t.startswith("./"):
            t = t[2:]
        targets.add(t)
    return targets


def _owning_index(section_dir: str, rel_page: str) -> str:
    """The ``index.md`` that should list ``rel_page``: the nearest one at or
    above the page's own directory, within the section.

    A section with no nested ``index.md`` always resolves to its root, which
    is why adding this changed nothing for design, dev and gallery.
    """
    parts = rel_page.split("/")[:-1]
    # A SUBSECTION's own index.md is listed by its PARENT, not by itself:
    # docs/guide/wfm-io/index.md belongs in docs/guide/index.md. Start the
    # search one level up so it cannot satisfy itself.
    if rel_page.rsplit("/", 1)[-1] == "index.md" and parts:
        parts.pop()
    while parts:
        cand = os.path.join(section_dir, *parts, "index.md")
        if os.path.isfile(cand):
            return cand
        parts.pop()
    return os.path.join(section_dir, "index.md")


def main() -> int:
    # index.md path (repo-relative) -> the pages it fails to list.
    missing: dict[str, list[str]] = {}
    for section, rel_dir in SECTIONS.items():
        section_dir = os.path.join(ROOT, rel_dir)
        should_list = _on_disk_pages(section_dir) | _nav_pages(section)
        for page in should_list:
            index_md = _owning_index(section_dir, page)
            # Links in an index are relative to THAT index, so compare on the
            # page's path relative to it rather than to the section root.
            owner_dir = os.path.dirname(index_md)
            rel_to_owner = os.path.relpath(
                os.path.join(section_dir, page), owner_dir
            ).replace(os.sep, "/")
            if rel_to_owner in _linked_pages(index_md):
                continue
            key = os.path.relpath(index_md, ROOT).replace(os.sep, "/")
            missing.setdefault(key, []).append(rel_to_owner)

    if not missing:
        print("Nav index coverage: OK — every page has an index.md bullet")
        return 0

    print(
        "Nav index coverage: pages missing from their index.md:\n",
        file=sys.stderr,
    )
    for rel_index in sorted(missing):
        print(f"  {rel_index}:", file=sys.stderr)
        for g in sorted(missing[rel_index]):
            print(f"    - add a bullet for {g}", file=sys.stderr)
    print(
        "\nEach page must be linked from the index.md NEAREST it.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
