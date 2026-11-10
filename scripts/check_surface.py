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
renders fails as stale.

Two homes, because the Docs job checks out one commit and has no base to
ratchet against (the same split ``user-docs-check`` makes):

* ``make docs-check``: the built-site check, ``--no-ratchet``;
* ``make lint`` (``surface-check``): the source rules and the ratchet,
  ``--no-site``.
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


#: Directories under a package that hold no API: never read for ``__all__``.
NOT_API = {"tests", "benchmarks", "examples"}


def declared_all(path: Path, root: Path) -> list[str] | None:
    """The literal ``__all__`` a module assigns, or None if it assigns none.

    ``root`` only shortens the path an error names, so CI output reads the
    same on every machine.

    Raises
    ------
    ValueError
        ``__all__`` is assigned other than once, at the top level, as one
        literal list: inside an ``if``/``try``, augmented, or computed. A
        gate cannot tell what such a module exports, so it refuses it
        rather than reading it as "nothing".
    """
    shown = path.relative_to(root)
    tree = ast.parse(path.read_text(encoding="utf-8"))
    stores = [
        n
        for n in ast.walk(tree)
        if isinstance(n, ast.Name)
        and n.id == "__all__"
        and isinstance(n.ctx, ast.Store)
    ]
    if not stores:
        return None
    top = [
        node
        for node in tree.body
        if (
            isinstance(node, ast.Assign)
            and any(getattr(t, "id", None) == "__all__" for t in node.targets)
        )
        or (
            isinstance(node, ast.AnnAssign)
            and getattr(node.target, "id", None) == "__all__"
            and node.value is not None
        )
    ]
    if len(stores) != 1 or len(top) != 1:
        raise ValueError(f"{shown}: __all__ is not one top-level assignment")
    try:
        return list(ast.literal_eval(top[0].value))
    except ValueError:
        raise ValueError(f"{shown}: __all__ is not a literal list") from None


def surface(root: Path) -> tuple[dict[str, list[str]], list[str]]:
    """``{package: [public export, ...]}``, and the source's own errors.

    The public surface is each top-level package's ``__all__``, so:

    * every package declares one; ``__all__ = []`` says it has no API (the
      ``specan`` and ``cli`` applications, the test tree), which autonav
      then has nothing to render for;
    * a submodule of a package WITH an API that declares its own
      ``__all__`` exports only names its package re-exports. Otherwise
      those names are public in no page: the reference renders packages,
      not submodules.
    """
    out: dict[str, list[str]] = {}
    errors: list[str] = []
    for init in sorted((root / "src" / PACKAGE).glob("*/__init__.py")):
        pkg = init.parent
        try:
            names = declared_all(init, root)
        except ValueError as e:
            errors.append(str(e))
            continue
        if names is None:
            errors.append(
                f"{init.relative_to(root)}: declares no __all__; list its "
                "public names, or write __all__ = [] for a package with no API"
            )
            continue
        public = sorted(n for n in names if not n.startswith("_"))
        if not public:
            continue  # no API, so none in its submodules either
        out[pkg.name] = public
        for sub in sorted(pkg.rglob("*.py")):
            rel = sub.relative_to(pkg)
            if sub == init or NOT_API & set(rel.parts[:-1]):
                continue
            try:
                own = declared_all(sub, root)
            except ValueError as e:
                errors.append(str(e))
                continue
            stray = sorted(
                n
                for n in own or []
                if not n.startswith("_") and n not in names
            )
            if stray:
                errors.append(
                    f"{sub.relative_to(root)}: exports {', '.join(stray)}, "
                    f"which {PACKAGE}.{pkg.name} does not re-export -- add "
                    "them to its __all__ (jm: the module's reexports)"
                )
    return out, errors


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
    ap.add_argument(
        "--no-site",
        action="store_true",
        help="source rules and the ratchet only (make lint: no built site)",
    )
    a = ap.parse_args(argv)
    root = a.root
    site = a.site or root / "site"
    if not a.no_site and not (site / "index.html").exists():
        print(
            f"surface-check: no built site at {site}; run `make docs` first",
            file=sys.stderr,
        )
        return 1

    found, errors = surface(root)
    lp = root / LIST
    listed = parse_list(lp.read_text(encoding="utf-8") if lp.exists() else "")
    gone: set[str] = set()
    if not a.no_site:
        gone = set(missing(found, site))
        errors += [
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
    total = sum(len(v) for v in found.values())
    where = (
        "source rules and ratchet"
        if a.no_site
        else f"{total - len(gone)} of {total} in the built reference"
    )
    print(
        f"surface-check: OK -- {total} public export(s), {where}; "
        f"{len(listed)} listed in {LIST}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
