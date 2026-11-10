#!/usr/bin/env python3
"""Every public Python export has its entry in the BUILT reference (#3000).

The Python reference is generated, not listed: mkdocs-api-autonav gives
each public package one page, ``site/reference/doppler/<module>/``, and
mkdocstrings renders that package's ``__all__`` onto it. A name reaches the
reference by existing. This gate checks the artifact, not the source: for
every public export (a name in a package's ``__all__`` that does not start
with ``_``) the built page must carry the anchor mkdocstrings gives it,
``id="doppler.<module>.<name>"``.

That one check catches each way a name goes missing: a package with no
page (an autonav exclude, a missing ``__init__``), an export the renderer
cannot resolve (a hand-written type with no class in its stub), and a
render that drops it.

The surface is read from the source with nothing but ``ast``: each
package's ``__all__`` is one literal list (jm writes it, or a person does),
and a package that computes it instead fails here rather than going
unchecked. No renderer model is needed to find a miss: a name the renderer
cannot resolve has no anchor, and the anchor is what this checks. Stdlib
only, so it runs on the floor Python like every other gate.

Today's misses are listed in ``docs/.surface-unrendered``, one dotted name
per line followed by the issue that removes it. The list only SHRINKS:
compared with the merge base, a new line fails, and a line whose name now
renders fails as stale. Run after ``make docs``:

    uv run python scripts/check_surface.py
"""

from __future__ import annotations

import argparse
import ast
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _gitbase import (
    BaseUnreadableError,
    added_since_base,
    in_git_repo,
)

ROOT = Path(__file__).resolve().parent.parent
SELF = "scripts/check_surface.py"
LIST = "docs/.surface-unrendered"
PACKAGE = "doppler"


def exports(root: Path) -> dict[str, list[str]]:
    """``{module: [public export, ...]}`` for every package with ``__all__``.

    A package without ``__all__`` declares no API (the ``specan`` and
    ``cli`` applications, the test tree), and autonav renders none for it.

    Raises
    ------
    ValueError
        A package assigns ``__all__`` other than once, as one literal list.
    """
    out: dict[str, list[str]] = {}
    for init in sorted((root / "src" / PACKAGE).glob("*/__init__.py")):
        tree = ast.parse(init.read_text(encoding="utf-8"))
        hits = [
            node
            for node in tree.body
            if isinstance(node, (ast.Assign, ast.AugAssign, ast.AnnAssign))
            and "__all__"
            in [
                getattr(t, "id", None)
                for t in (
                    node.targets
                    if isinstance(node, ast.Assign)
                    else [node.target]
                )
            ]
        ]
        if not hits:
            continue
        if len(hits) != 1 or not isinstance(hits[0], ast.Assign):
            raise ValueError(f"{init}: __all__ is not one plain assignment")
        try:
            names = ast.literal_eval(hits[0].value)
        except ValueError:
            raise ValueError(
                f"{init}: __all__ is not a literal list"
            ) from None
        public = sorted(n for n in names if not n.startswith("_"))
        if public:
            out[init.parent.name] = public
    return out


def missing(surface: dict[str, list[str]], site: Path) -> list[str]:
    """Every ``doppler.<module>.<name>`` whose anchor the built site lacks."""
    out: list[str] = []
    for mod, names in sorted(surface.items()):
        page = site / "reference" / PACKAGE / mod / "index.html"
        html = page.read_text(encoding="utf-8") if page.exists() else ""
        out += [
            f"{PACKAGE}.{mod}.{n}"
            for n in names
            if f'id="{PACKAGE}.{mod}.{n}"' not in html
        ]
    return out


def parse_list(text: str) -> set[str]:
    """The dotted names a ``.surface-unrendered`` text lists."""
    return {
        line.split()[0]
        for line in text.splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--site", type=Path, default=None)
    ap.add_argument("--base", default="origin/main")
    ap.add_argument("--no-ratchet", action="store_true")
    a = ap.parse_args(argv)
    root = a.root
    site = a.site or root / "site"
    if not (site / "index.html").exists():
        print(
            f"surface-check: no built site at {site}; run `make docs` first",
            file=sys.stderr,
        )
        return 1

    try:
        surface = exports(root)
    except ValueError as e:
        print(f"surface-check: FAILED\n  {e}", file=sys.stderr)
        return 1
    gone = set(missing(surface, site))
    lp = root / LIST
    listed = parse_list(lp.read_text(encoding="utf-8") if lp.exists() else "")

    errors = [
        f"{n}: public, but not in the built reference -- give it a "
        "docstring and a stub class, or take it out of __all__"
        for n in sorted(gone - listed)
    ]
    errors += [
        f"{LIST}: '{n}' renders now; delete the line"
        for n in sorted(listed - gone)
    ]
    if not a.no_ratchet and in_git_repo(root):
        try:
            grown = added_since_base(
                root, a.base, LIST, sorted(listed), parse_list, since=SELF
            )
        except BaseUnreadableError:
            errors.append(f"cannot read {LIST} at {a.base}; ratchet unchecked")
            grown = []
        errors += [
            f"{LIST}: '{n}' is new; the list only shrinks" for n in grown
        ]

    if errors:
        print("surface-check: FAILED", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1
    total = sum(len(v) for v in surface.values())
    print(
        f"surface-check: OK -- {total - len(gone)} of {total} public "
        f"export(s) in the built reference; {len(listed)} listed in {LIST}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
