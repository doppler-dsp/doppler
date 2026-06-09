#!/usr/bin/env python3
"""Fail when a public Python symbol has no mkdocstrings page.

The Python API reference is hand-curated `::: doppler.<module>.<symbol>`
directives under ``docs/api/``. It is easy to add a new class/function (or a
whole module) and forget to document it — that is how ``doppler.spectral``'s
``Detector`` shipped undocumented. This script is the backstop: it enumerates
every public symbol (each module's ``__all__``, or the symbols actually defined
in a no-``__all__`` submodule) and checks that each is referenced by a ``:::``
directive somewhere under ``docs/api/``.

Usage
-----
    python scripts/check_api_docs.py            # report + exit 1 if any gaps
    python scripts/check_api_docs.py --scaffold doppler.foo
                                                # print a stub page + nav line

Intentional exclusions go in ``docs/api/.api-coverage-ignore`` (one
fully-qualified name per line, ``#`` comments allowed).
"""

from __future__ import annotations

import importlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API_DIR = os.path.join(ROOT, "docs", "api")
IGNORE_FILE = os.path.join(API_DIR, ".api-coverage-ignore")

# Subpackages that are applications / entry points, not an importable API.
SKIP_TOP = {"cli", "specan"}
# No-``__all__`` submodules whose public surface should still be covered.
EXTRA_MODULES = ["doppler.wfmgen.compose", "doppler.wfmgen.readback"]


def public_symbols(mod) -> list[str]:
    """The public names of a module: ``__all__`` if present, else the names
    actually defined there (filtering imports like numpy/os)."""
    allv = getattr(mod, "__all__", None)
    if allv is not None:
        return list(allv)
    out = []
    for name in dir(mod):
        if name.startswith("_"):
            continue
        obj = getattr(mod, name)
        if getattr(obj, "__module__", None) == mod.__name__:
            out.append(name)
    return out


def discover_modules() -> list[str]:
    """Top-level ``doppler.<sub>`` packages with a public API, plus the known
    no-``__all__`` submodules."""
    import doppler

    pkgdir = os.path.dirname(doppler.__file__)
    mods = []
    for entry in sorted(os.listdir(pkgdir)):
        if entry.startswith("_") or entry in SKIP_TOP:
            continue
        if not os.path.isdir(os.path.join(pkgdir, entry)):
            continue
        if entry in ("tests", "benchmarks"):
            continue
        name = f"doppler.{entry}"
        try:
            mod = importlib.import_module(name)
        except Exception as exc:  # pragma: no cover - import failure is fatal
            print(f"  cannot import {name}: {exc}", file=sys.stderr)
            continue
        if public_symbols(mod):  # has a public surface
            mods.append(name)
    return mods + EXTRA_MODULES


def documented_names() -> set[str]:
    """Every identifier that appears anywhere under ``docs/api/``.

    A symbol counts as documented if its name shows up at all — in a ``:::``
    directive, a prose mention, or a code example. The bar is deliberately low:
    the goal is to catch a *wholly undocumented* surface (a new class no page
    references), not to mandate a particular documentation style.
    """
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
    for modname in discover_modules():
        mod = importlib.import_module(modname)
        for sym in public_symbols(mod):
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
    mod = importlib.import_module(modname)
    short = modname.split(".")[-1]
    print(f"# --- docs/api/python-{short}.md ---")
    print(f"# Python {short.title()} API\n")
    print(f"The `{modname}` module.\n")
    for sym in public_symbols(mod):
        print(f"::: {modname}.{sym}\n")
    print("# --- add to mkdocs.yml nav (API Reference): ---")
    print(f'      - "Python: {short.title()}": api/python-{short}.md')
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
