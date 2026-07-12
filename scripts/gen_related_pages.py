#!/usr/bin/env python3
"""Generate a "## Related pages" block on each ``docs/api/*.md`` page.

Every doppler class/function is documented on an ``docs/api/*.md`` page
(enforced by ``check_api_docs.py``), but a reader landing there has no way
to discover the gallery walkthrough, guide, or design doc that also talks
about it -- those pages already reference the class by name (e.g.
``docs/gallery/symsync.md`` opens with
`` [`track.SymbolSync`](../api/python-track.md) ``), the convention just
isn't aggregated anywhere. This script closes that gap: for every symbol
documented on an api page, it scans ``docs/gallery/``, ``docs/guide/``,
``docs/design/``, and ``docs/dev/`` for a backtick-span or link-text mention
of that exact symbol name, and writes the matches into a marked block.

Deliberately backtick/link-text scoped, not a bare word-boundary match --
several doppler class names double as common English words (``Plan``,
``Segment``, ``Reader``, ``Writer``, ``Push``, ``Pull``, ``Timeline``,
``Corr``, ``Composer``); a spike against the real corpus confirmed the
stricter scope produces zero false positives across all of them.

The generated block never touches an existing hand-written ``## See also``
section -- it is always appended as a new, separate heading at the end of
the page. A page whose symbols have no matches anywhere gets no block at
all (no empty heading clutter).

Escape hatch for a genuinely un-name-matchable editorial link: add an entry
to ``docs/api/.related-pages-manual.yml`` (sparingly -- prefer naming the
class in backticks on the source page instead, when possible).

Usage
-----
    python scripts/gen_related_pages.py --write   # regenerate every block
    python scripts/gen_related_pages.py --check   # exit 1 on any drift
"""

from __future__ import annotations

import os
import re
import sys

import yaml
from _pydocs import discover

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API_DIR = os.path.join(ROOT, "docs", "api")
MANUAL_FILE = os.path.join(API_DIR, ".related-pages-manual.yml")

# Section dir (relative to ROOT) -> family label, in display order. A
# page's dev/ mentions surface under "Contributing", matching the family
# name docs/start-here.md already uses for that section.
SCAN_FAMILIES = [
    ("docs/gallery", "Gallery"),
    ("docs/guide", "Guides"),
    ("docs/design", "Design"),
    ("docs/dev", "Contributing"),
]

DIRECTIVE_RE = re.compile(r"^::: (doppler(?:\.[\w]+)+)\s*$", re.MULTILINE)
# A hand-written page (no mkdocstrings directive at all, e.g.
# python-telemetry.md's Telemetry -- a no_generate/hand-owned binding)
# still documents its symbol under a "## `Symbol`" heading; only counted
# when the name is also a real discovered symbol, to avoid picking up an
# unrelated heading that happens to be in backticks.
HEADING_RE = re.compile(r"^#{2,3} `(\w+)`\s*$", re.MULTILINE)
BACKTICK_RE = re.compile(r"`([^`\n]+)`")
LINK_TEXT_RE = re.compile(r"\[([^\]\n]+)\]\([^)\n]+\)")
TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
TITLE_RE = re.compile(r"^#\s+(.+?)\s*$", re.MULTILINE)

START_MARKER = "<!-- related-pages:start -->"
END_MARKER = "<!-- related-pages:end -->"


def _page_title(path: str) -> str:
    """The page's first ``# `` heading, or its filename if there isn't one."""
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    m = TITLE_RE.search(text)
    return m.group(1) if m else os.path.basename(path)


def _mentioned_tokens(path: str) -> set[str]:
    """Every identifier appearing inside a backtick span or a markdown
    link's text on this page -- the deliberately narrow scan scope."""
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    spans = BACKTICK_RE.findall(text) + LINK_TEXT_RE.findall(text)
    tokens: set[str] = set()
    for span in spans:
        tokens.update(TOKEN_RE.findall(span))
    return tokens


def _scan_corpus() -> dict[str, list[tuple[str, str, str]]]:
    """symbol name -> [(family, rel_path_from_api_dir, page_title), ...],
    scanning every ``*.md`` under each family dir except ``archive/``."""
    hits: dict[str, list[tuple[str, str, str]]] = {}
    for rel_dir, family in SCAN_FAMILIES:
        section_dir = os.path.join(ROOT, rel_dir)
        for dirpath, dirnames, filenames in os.walk(section_dir):
            dirnames[:] = [d for d in dirnames if d != "archive"]
            for fn in sorted(filenames):
                if not fn.endswith(".md"):
                    continue
                path = os.path.join(dirpath, fn)
                tokens = _mentioned_tokens(path)
                if not tokens:
                    continue
                title = _page_title(path)
                rel_from_api = os.path.relpath(path, API_DIR).replace(
                    os.sep, "/"
                )
                for sym in tokens:
                    hits.setdefault(sym, []).append(
                        (family, rel_from_api, title)
                    )
    return hits


def _load_manual() -> dict[str, list[dict]]:
    if not os.path.exists(MANUAL_FILE):
        return {}
    with open(MANUAL_FILE, encoding="utf-8") as fh:
        data = yaml.safe_load(fh) or {}
    return data


def _page_symbols(path: str, known_symbols: set[str]) -> list[str]:
    """Symbol names documented on this api page: its ``::: doppler...``
    mkdocstrings directives, plus any ``## `Symbol` `` heading for a
    hand-written page with no directive at all (deduped, order preserved)."""
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    out: list[str] = []
    seen: set[str] = set()
    for m in DIRECTIVE_RE.findall(text):
        sym = m.split(".")[-1]
        if sym not in seen:
            seen.add(sym)
            out.append(sym)
    for sym in HEADING_RE.findall(text):
        if sym in known_symbols and sym not in seen:
            seen.add(sym)
            out.append(sym)
    return out


def _build_block(
    page_fn: str, symbols: list[str], corpus: dict, manual: dict
) -> str | None:
    """The full ``## Related pages`` section text, or None if nothing to
    show for any symbol on this page."""
    by_family: dict[str, dict[str, str]] = {
        family: {} for _, family in SCAN_FAMILIES
    }
    for sym in symbols:
        for family, rel_path, title in corpus.get(sym, []):
            by_family[family][rel_path] = title
    for entry in manual.get(page_fn, []):
        by_family.setdefault(entry["family"], {})
        by_family[entry["family"]][entry["path"]] = entry["title"]

    if not any(by_family.values()):
        return None

    # Blank lines around the markers: mdformat (pre-commit) treats an HTML
    # comment as a block-level element and inserts them anyway, so match
    # that house style directly -- otherwise every --write is immediately
    # re-dirtied by pre-commit, and a --check right after would false-fail.
    lines = ["## Related pages", "", START_MARKER, ""]
    for _, family in SCAN_FAMILIES:
        pages = by_family.get(family)
        if not pages:
            continue
        links = ", ".join(
            f"[{title}]({path})" for path, title in sorted(pages.items())
        )
        lines.append(f"**{family}** — {links}")
    lines.append("")
    lines.append(END_MARKER)
    return "\n".join(lines) + "\n"


def _strip_existing_block(text: str) -> str:
    """Remove a previously-generated block (heading + marked region) so a
    fresh one can be appended; leaves everything else untouched."""
    pattern = re.compile(
        r"\n*## Related pages\n\n"
        + re.escape(START_MARKER)
        + r".*?"
        + re.escape(END_MARKER)
        + r"\n?",
        re.DOTALL,
    )
    return pattern.sub("", text).rstrip("\n") + "\n"


def main() -> int:
    if "--write" not in sys.argv and "--check" not in sys.argv:
        print(__doc__)
        return 1
    check_only = "--check" in sys.argv

    corpus = _scan_corpus()
    manual = _load_manual()
    known_symbols = {sym for _, syms in discover() for sym in syms}

    drift: list[str] = []
    for fn in sorted(os.listdir(API_DIR)):
        if not fn.endswith(".md"):
            continue
        path = os.path.join(API_DIR, fn)
        symbols = _page_symbols(path, known_symbols)
        if not symbols:
            continue
        block = _build_block(fn, symbols, corpus, manual)

        with open(path, encoding="utf-8") as fh:
            original = fh.read()
        stripped = _strip_existing_block(original)
        new_text = stripped if block is None else f"{stripped}\n{block}"

        if new_text == original:
            continue
        if check_only:
            drift.append(fn)
        else:
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(new_text)

    if check_only:
        if drift:
            print(
                "Related pages drift — run "
                "`python scripts/gen_related_pages.py --write`:",
                file=sys.stderr,
            )
            for fn in drift:
                print(f"  {fn}", file=sys.stderr)
            return 1
        print("Related pages: OK — up to date")
        return 0

    print("Related pages: regenerated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
