#!/usr/bin/env python3
"""Every public class owes its user story; every example owes a gallery call.

The rule lives in ``docs/dev/contributing/adding-algorithms.md``, phase 9
("The user story"). This is its gate. ``DelayDoppler`` shipped in v0.78.0
with no guide, no gallery page and no nav entry, and every existing docs
check passed, because each one verifies a page that already exists. None
asks whether a page that should exist does.

For every public class (a ``class`` in its package's sources or stubs that
the package exports through ``__all__``):

``api``
    A real entry on a ``docs/api/*.md`` page: a ``::: doppler.<mod>...Cls``
    directive, or a ``## `Cls` `` heading on a hand-written page. A bare
    mention in prose is not an entry.
``guide``
    At least one ``docs/guide/**`` page names it in a backtick span or a
    link's text, the scope ``gen_related_pages.py`` uses (the same scanner,
    so the two cannot disagree on what a mention is).
``example``
    At least one ``src/doppler/examples/*.py`` uses the name in code.

A record type (a ``record_name`` in the jm manifests: ``ToneMetrics``,
``SyncHit``, ...) owes ``api`` only. It is what a method returns, so its
story is the story of the class that returns it.

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

from _gitbase import BaseUnreadableError, added_since_base, in_git_repo
from _pydocs import EXTRA_MODULES, discover
from gen_related_pages import DIRECTIVE_RE, _mentioned_tokens

ROOT = Path(__file__).resolve().parent.parent
EXEMPT = "docs/.user-docs-exempt"
NO_FIGURE = "src/doppler/examples/.no-figure"
EXAMPLES = "src/doppler/examples"
SELF = "scripts/check_user_docs.py"

CLASS_RULES = ("api", "guide", "example")
RECORD_RULES = ("api",)
RECORD_RE = re.compile(r'^\s*record_name\s*=\s*"(\w+)"', re.MULTILINE)
ENTRY_RE = re.compile(r"^(\S+)\s+(api|guide|example|gallery):\s*(\S.*)$")

FIX = {
    "api": "add a `::: {key}` directive to its docs/api/ page",
    "guide": "name it in backticks in the docs/guide/ page for its task",
    "example": "use it in a src/doppler/examples/ script",
    "gallery": (
        "add it to GALLERY_SCRIPTS with a figure, or say why it has none "
        f"in {NO_FIGURE}"
    ),
}


def _class_names(paths: list[Path]) -> set[str]:
    """Top-level ``class`` names across these sources and stubs."""
    out: set[str] = set()
    for p in paths:
        try:
            tree = ast.parse(p.read_text(encoding="utf-8"))
        except (OSError, SyntaxError):
            continue
        out.update(n.name for n in tree.body if isinstance(n, ast.ClassDef))
    return out


def public_classes(root: Path) -> dict[str, str]:
    """``{dotted name: bare name}`` for every exported class."""
    pkg = root / "src" / "doppler"
    extra = dict(EXTRA_MODULES)
    out: dict[str, str] = {}
    for mod, syms in discover(str(pkg)):
        if mod in extra:
            files = [pkg / extra[mod]]
        else:
            d = pkg / mod.split(".", 1)[1]
            files = sorted(d.glob("*.py")) + sorted(d.glob("*.pyi"))
        classes = _class_names(files)
        for s in syms:
            if s in classes and not s.startswith("_"):
                out[f"{mod}.{s}"] = s
    return out


def record_names(root: Path) -> set[str]:
    """Every ``record_name`` the jm manifests declare."""
    files = [
        root / "just-makeit.toml",
        *sorted((root / "objects").glob("*.toml")),
    ]
    out: set[str] = set()
    for f in files:
        if f.exists():
            out.update(RECORD_RE.findall(f.read_text(encoding="utf-8")))
    return out


HEAD_LINE_RE = re.compile(r"^#{2,3} (.*)$", re.MULTILINE)
MEMBER_RE = re.compile(r"^\s+- (\w+)\s*$")


def _directive_members(text: str) -> set[str]:
    """Dotted names a ``::: doppler.<mod>`` MODULE directive renders.

    mkdocstrings renders the ``members:`` list under ``options:`` when one
    is given; each becomes ``<mod>.<member>``. A module directive with no
    list renders every public member, which ``has_api`` answers from the
    bare module path instead.
    """
    out: set[str] = set()
    lines = text.splitlines()
    for i, line in enumerate(lines):
        m = DIRECTIVE_RE.match(line)
        if not m:
            continue
        in_members = False
        for nxt in lines[i + 1 :]:
            if nxt.strip() and not nxt.startswith(" "):
                break
            if nxt.strip() == "members:":
                in_members = True
                continue
            mm = MEMBER_RE.match(nxt)
            if in_members and mm:
                out.add(f"{m[1]}.{mm[1]}")
            elif nxt.strip().endswith(":"):
                in_members = False
    return out


def api_entries(root: Path) -> tuple[set[str], set[str]]:
    """(directive dotted paths, names in a heading's backticks) in docs/api.

    A heading counts for every name in its backticks, so
    "## `Capture` and `MemoryCapture`" is an entry for both, and for its
    leading word, so a hand-written "## MpskReceiver — ..." section is one.
    """
    dotted: set[str] = set()
    heads: set[str] = set()
    for p in sorted((root / "docs" / "api").glob("*.md")):
        text = p.read_text(encoding="utf-8")
        dotted.update(DIRECTIVE_RE.findall(text))
        dotted |= _directive_members(text)
        for h in HEAD_LINE_RE.findall(text):
            heads.update(re.findall(r"`(\w+)`", h))
            lead = re.match(r"(\w+)\b", h)
            if lead:
                heads.add(lead[1])
    return dotted, heads


def has_api(key: str, bare: str, dotted: set[str], heads: set[str]) -> bool:
    """A directive for this class (at its path, or a deeper re-export of
    the same module), or a heading naming it."""
    mod = key.rsplit(".", 1)[0]
    if bare in heads:
        return True
    return any(
        d == key or (d.startswith(mod + ".") and d.rsplit(".", 1)[1] == bare)
        for d in dotted
    )


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
    dotted, heads = api_entries(root)
    guide = guide_tokens(root)
    examples = example_names(root)
    used = set().union(*examples.values()) if examples else set()

    found: set[tuple[str, str]] = set()
    for key, bare in classes.items():
        rules = RECORD_RULES if bare in records else CLASS_RULES
        if "api" in rules and not has_api(key, bare, dotted, heads):
            found.add((key, "api"))
        if "guide" in rules and bare not in guide:
            found.add((key, "guide"))
        if "example" in rules and bare not in used:
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
