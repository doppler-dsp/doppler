#!/usr/bin/env python3
"""Fail when a public Python symbol has no mkdocstrings page.

The Python API reference is hand-curated `::: doppler.<module>.<symbol>`
directives under ``docs/api/``. It is easy to add a new class/function (or a
whole module) and forget to document it — that is how ``doppler.spectral``'s
``Detector`` shipped undocumented. This script is the backstop: it enumerates
every public symbol (each module's ``__all__``, or the symbols defined in a
no-``__all__`` submodule) and checks that each is referenced somewhere under
``docs/api/`` (a ``:::`` directive, prose, or an example).

It works by **static analysis** (``ast``) — it never imports ``doppler``, so it
runs in any environment (e.g. the docs CI job, which doesn't build the C
extensions; mkdocstrings itself introspects statically via griffe).

Usage
-----
    python scripts/check_api_docs.py            # report + exit 1 if any gaps
    python scripts/check_api_docs.py --scaffold doppler.foo
                                                # print a stub page + nav line

Intentional exclusions go in ``docs/api/.api-coverage-ignore`` (one
fully-qualified name per line, ``#`` comments allowed).
"""

from __future__ import annotations

import ast
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PKG_DIR = os.path.join(ROOT, "src", "doppler")
API_DIR = os.path.join(ROOT, "docs", "api")
IGNORE_FILE = os.path.join(API_DIR, ".api-coverage-ignore")

# Subpackages that are applications / entry points, not an importable API.
SKIP_TOP = {"cli", "specan", "tests", "benchmarks"}
# No-``__all__`` submodules whose public surface should still be covered,
# as (dotted name, path-relative-to-PKG_DIR).
EXTRA_MODULES = [
    ("doppler.wfm.compose", "wfm/compose.py"),
    ("doppler.wfm.readback", "wfm/readback.py"),
]


def _all_list(path: str) -> list[str] | None:
    """The literal ``__all__`` list from a Python file, or None if absent."""
    try:
        tree = ast.parse(open(path, encoding="utf-8").read())
    except (OSError, SyntaxError):
        return None
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "__all__" for t in node.targets
        ):
            if isinstance(node.value, (ast.List, ast.Tuple)):
                return [
                    e.value
                    for e in node.value.elts
                    if isinstance(e, ast.Constant) and isinstance(e.value, str)
                ]
    return None


def _public_defs(path: str) -> list[str]:
    """Top-level public class/def names defined in a module file."""
    tree = ast.parse(open(path, encoding="utf-8").read())
    out = []
    for node in tree.body:
        if isinstance(
            node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)
        ) and not node.name.startswith("_"):
            out.append(node.name)
    return out


def discover() -> list[tuple[str, list[str]]]:
    """``[(module_dotted_name, [public_symbols])]`` via static parsing."""
    mods: list[tuple[str, list[str]]] = []
    for entry in sorted(os.listdir(PKG_DIR)):
        if entry in SKIP_TOP or entry.startswith("_"):
            continue
        init = os.path.join(PKG_DIR, entry, "__init__.py")
        if not os.path.isfile(init):
            continue
        allv = _all_list(init)
        if allv:
            mods.append((f"doppler.{entry}", allv))
    for name, rel in EXTRA_MODULES:
        p = os.path.join(PKG_DIR, rel)
        if os.path.exists(p):
            mods.append((name, _public_defs(p)))
    return mods


def documented_names() -> set[str]:
    """Every identifier that appears anywhere under ``docs/api/`` — a ``:::``
    directive, a prose mention, or a code example. The bar is deliberately low:
    catch a *wholly undocumented* surface, not mandate a documentation style."""
    tok = re.compile(r"[A-Za-z_]\w+")
    names: set[str] = set()
    for fn in os.listdir(API_DIR):
        if not fn.endswith(".md"):
            continue
        text = open(os.path.join(API_DIR, fn), encoding="utf-8").read()
        names.update(tok.findall(text))
    return names


def load_ignore() -> set[str]:
    if not os.path.exists(IGNORE_FILE):
        return set()
    out = set()
    for line in open(IGNORE_FILE, encoding="utf-8"):
        line = line.split("#", 1)[0].strip()
        if line:
            out.add(line)
    return out


def main() -> int:
    if len(sys.argv) == 3 and sys.argv[1] == "--scaffold":
        return scaffold(sys.argv[2])

    documented = documented_names()
    ignore = load_ignore()
    missing: list[str] = []
    for modname, syms in discover():
        for sym in syms:
            qual = f"{modname}.{sym}"
            if qual in ignore or sym in documented:
                continue
            missing.append(qual)

    if not missing:
        print("API docs coverage: OK — every public symbol is documented")
        return 0
    print("API docs coverage: undocumented public symbols:\n", file=sys.stderr)
    for q in missing:
        print(f"  {q}", file=sys.stderr)
    print(
        "\nEach must be named somewhere under docs/api/ (a ::: directive, prose,"
        " or an example). Run `python scripts/check_api_docs.py --scaffold "
        "<module>` for a stub, or add intentional exclusions to "
        f"{os.path.relpath(IGNORE_FILE, ROOT)}.",
        file=sys.stderr,
    )
    return 1


def scaffold(modname: str) -> int:
    """Print a stub page + the nav line for a module (does not write files)."""
    syms = dict(discover()).get(modname)
    if syms is None:
        print(f"unknown module {modname!r}", file=sys.stderr)
        return 1
    short = modname.split(".")[-1]
    print(f"# --- docs/api/python-{short}.md ---")
    print(f"# Python {short.title()} API\n")
    print(f"The `{modname}` module.\n")
    for sym in syms:
        print(f"::: {modname}.{sym}\n")
    print("# --- add to mkdocs.yml nav (API Reference): ---")
    print(f'      - "Python: {short.title()}": api/python-{short}.md')
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
