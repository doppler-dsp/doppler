#!/usr/bin/env python3
"""Every public class owes its user story; every example owes a gallery call.

The rule lives in ``docs/dev/contributing/adding-algorithms.md``, phase 9
("The user story"). This is its gate. ``DelayDoppler`` shipped in v0.78.0
with no guide, no gallery page and no nav entry, and every existing docs
check passed, because each one verifies a page that already exists. None
asks whether a page that should exist does.

For every public class (a ``class`` in its package's sources or stubs that
the package exports through ``__all__``):

``guide``
    At least one ``docs/guide/**`` page names it in a backtick span or a
    link's text (``_mentioned_tokens``).
``example``
    At least one ``src/doppler/examples/*.py`` uses the name in code.

A record type (a ``record_name`` in the jm manifests: ``ToneMetrics``,
``SyncHit``, ...) owes neither. It is what a method returns, so its story
is the story of the class that returns it, and its reference entry is the
generated one every public name gets (``check_surface.py``, #3000).

For every example script:

``gallery``
    It is a gallery script (in ``GALLERY_SCRIPTS``, passed in by
    ``make user-docs-check``), or ``src/doppler/examples/.no-figure`` says
    why it has no figure. That list is a decision, so it may grow; a
    reason is mandatory and an entry naming no script fails.

Today's gaps sit in ``docs/.user-docs-exempt``, one ``<key> <rule>: <why>``
per line. It is a RATCHET: compared with the merge base, it may only
shrink, and an entry whose gap has closed (or whose class is gone) fails
as stale, so the list is always exactly the work left.

The shrink-only comparison needs the base ref, so it runs on ``make lint``.
docs-check, whose CI job checks out no base, passes ``--no-ratchet`` and
checks coverage and staleness only.

Usage
-----
    python scripts/check_user_docs.py [--root DIR] [--base REF] GALLERY...
"""

from __future__ import annotations

import argparse
import ast
import os
import re
import sys
from pathlib import Path
from typing import TYPE_CHECKING

from _gitbase import BaseUnreadableError, added_since_base, in_git_repo
from _pydocs import kinds

# What a guide "mentions": an identifier inside a backtick span or a link's
# text -- deliberately narrow, so prose that happens to say "correlator" is
# not a mention of ``Correlator``.
BACKTICK_RE = re.compile(r"`([^`\n]+)`")
# A link's text, inline `[text](url)` or a cross-reference `[text][id]`:
# the generated reference is reached by the second (#3000).
LINK_TEXT_RE = re.compile(r"\[([^\]\n]+)\](?:\([^)\n]+\)|\[[^\]\n]*\])")
TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def _mentioned_tokens(path: str) -> set[str]:
    """Every identifier in a backtick span or a link's text on this page."""
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    tokens: set[str] = set()
    for span in BACKTICK_RE.findall(text) + LINK_TEXT_RE.findall(text):
        tokens.update(TOKEN_RE.findall(span))
    return tokens


if TYPE_CHECKING:
    from collections.abc import Iterator

ROOT = Path(__file__).resolve().parent.parent
EXEMPT = "docs/.user-docs-exempt"
NO_FIGURE = "src/doppler/examples/.no-figure"
EXAMPLES = "src/doppler/examples"
SELF = "scripts/check_user_docs.py"

ENTRY_RE = re.compile(r"^(\S+)\s+(guide|example|gallery):\s*(\S.*)$")

FIX = {
    "guide": "name it in backticks in the docs/guide/ page for its task",
    "example": "use it in a src/doppler/examples/ script",
    "gallery": (
        "add it to GALLERY_SCRIPTS with a figure, or say why it has none "
        f"in {NO_FIGURE}"
    ),
}


def public_classes(root: Path) -> dict[str, str]:
    """``{dotted name: bare name}`` for every exported class."""
    return {
        f"{mod}.{s}": s
        for mod, ks in kinds(str(root / "src" / "doppler")).items()
        for s, k in ks.items()
        if k == "class" and not s.startswith("_")
    }


def _record_values(node: object) -> Iterator[str]:
    """Every ``record_name`` value anywhere under a manifest table."""
    if isinstance(node, dict):
        for k, v in node.items():
            if k == "record_name" and isinstance(v, str):
                yield v
            else:
                yield from _record_values(v)
    elif isinstance(node, list):
        for v in node:
            yield from _record_values(v)


def record_names(root: Path) -> set[str]:
    """Every ``record_name`` the jm manifest declares.

    Read through jm's public ``load_manifest``: the root merged with every
    fragment its ``include`` names (``objects/``, ``modules/``), so a new
    fragment directory is read without an edit here, and a ``record_name``
    in a comment is not one.

    Refuses rather than returning an empty set: a missing or unusable
    manifest, or one declaring no record at all, would otherwise check
    every record type as a class and report a clean tree for the wrong
    reason (doppler#2751).
    """
    from just_makeit import load_manifest

    try:
        manifest = load_manifest(root)
    except (OSError, ValueError) as e:
        raise SystemExit(f"check_user_docs: the manifest: {e}") from e
    out = set(_record_values(manifest))
    if not out:
        raise SystemExit(
            f"check_user_docs: {root}'s manifest declares no record_name; "
            "refusing to check every record type as a class"
        )
    return out


def guide_tokens(root: Path) -> set[str]:
    """Every name a docs/guide page mentions, outside archive/."""
    out: set[str] = set()
    for dirpath, dirnames, filenames in os.walk(root / "docs" / "guide"):
        dirnames[:] = [d for d in dirnames if d != "archive"]
        for fn in filenames:
            if fn.endswith(".md"):
                out |= _mentioned_tokens(os.path.join(dirpath, fn))
    return out


def example_names(root: Path) -> dict[str, set[str]]:
    """``{script filename: identifiers its code uses}``."""
    out: dict[str, set[str]] = {}
    for p in sorted((root / EXAMPLES).glob("*.py")):
        if p.name.startswith("_"):
            continue  # a helper (_extra.py), not a script
        try:
            tree = ast.parse(p.read_text(encoding="utf-8"))
        except SyntaxError:
            out[p.name] = set()
            continue
        names: set[str] = set()
        for n in ast.walk(tree):
            if isinstance(n, ast.Name):
                names.add(n.id)
            elif isinstance(n, ast.Attribute):
                names.add(n.attr)
            elif isinstance(n, ast.alias):
                names.add(n.name.rsplit(".", 1)[-1])
        out[p.name] = names
    return out


def parse_entries(text: str) -> list[tuple[str, str, str]]:
    """``[(key, rule, why)]`` from an exempt-style list; comments skipped.

    A malformed line comes back with an empty rule so the caller can
    report it rather than silently skip it.
    """
    out: list[tuple[str, str, str]] = []
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        m = ENTRY_RE.match(s)
        out.append((m[1], m[2], m[3]) if m else (s, "", ""))
    return out


def parse_no_figure(text: str) -> dict[str, str]:
    """``{script: reason}`` from ``.no-figure`` (``script.py: reason``)."""
    out: dict[str, str] = {}
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        name, _, why = s.partition(":")
        out[name.strip()] = why.strip()
    return out


def gaps(
    root: Path, gallery: set[str]
) -> tuple[set[tuple[str, str]], list[str]]:
    """Every (key, rule) owed and missing, plus .no-figure's own errors."""
    classes = public_classes(root)
    records = record_names(root)
    guide = guide_tokens(root)
    examples = example_names(root)
    used = set().union(*examples.values()) if examples else set()

    found: set[tuple[str, str]] = set()
    for key, bare in classes.items():
        if bare in records:
            continue
        if bare not in guide:
            found.add((key, "guide"))
        if bare not in used:
            found.add((key, "example"))

    errors: list[str] = []
    nf_path = root / NO_FIGURE
    no_fig = (
        parse_no_figure(nf_path.read_text(encoding="utf-8"))
        if nf_path.exists()
        else {}
    )
    for name, why in sorted(no_fig.items()):
        if name not in examples:
            errors.append(f"{NO_FIGURE}: {name} names no example script")
        elif not why:
            errors.append(f"{NO_FIGURE}: {name} has no reason")
        elif name in gallery:
            errors.append(
                f"{NO_FIGURE}: {name} is a gallery script; drop its line"
            )
    for name in examples:
        if name not in gallery and name not in no_fig:
            found.add((name, "gallery"))
    return found, errors


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--base", default="origin/main")
    ap.add_argument(
        "--no-ratchet",
        action="store_true",
        help="check coverage only; for a checkout with no base ref (the "
        "docs job). The ratchet's home is `make lint`.",
    )
    ap.add_argument("gallery", nargs="*", help="the GALLERY_SCRIPTS paths")
    a = ap.parse_args()
    root = a.root.resolve()
    gallery = {Path(g).name for g in a.gallery}

    found, errors = gaps(root, gallery)

    ex_path = root / EXEMPT
    ex_text = ex_path.read_text(encoding="utf-8") if ex_path.exists() else ""
    entries = parse_entries(ex_text)
    listed = set()
    for key, rule, _why in entries:
        if not rule:
            errors.append(f"{EXEMPT}: malformed line: {key}")
            continue
        listed.add((key, rule))

    for key, rule in sorted(found - listed):
        errors.append(f"{key}: no {rule} -- {FIX[rule].format(key=key)}")
    for key, rule in sorted(listed - found):
        errors.append(
            f"{EXEMPT}: '{key} {rule}' is stale (done, or the class is "
            "gone); delete the line"
        )

    if not a.no_ratchet and in_git_repo(root):
        try:
            grown = added_since_base(
                root,
                a.base,
                EXEMPT,
                sorted(listed),
                lambda t: {(k, r) for k, r, _ in parse_entries(t) if r},
                since=SELF,
            )
        except BaseUnreadableError:
            errors.append(
                f"cannot read {EXEMPT} at {a.base}; the ratchet is unchecked"
            )
            grown = []
        for key, rule in grown:
            errors.append(
                f"{EXEMPT}: '{key} {rule}' is new; the list only shrinks. "
                f"Do the work instead: {FIX[rule].format(key=key)}"
            )

    if errors:
        print("user-docs-check: FAILED", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1
    print(
        f"user-docs-check: OK -- every class and example accounted for; "
        f"{len(listed)} gap(s) left in {EXEMPT}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
