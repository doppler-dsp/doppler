"""Shared static-analysis helpers for discovering doppler's public API.

Both ``check_api_docs.py`` (is every symbol documented?) and
``gen_related_pages.py`` (what does the rest of the docs say about each
symbol?) need the same answer to "what is the public Python API" -- this
module is the one AST walk both scripts build on, so neither can drift
from the other's idea of what counts as public.

Static analysis only (``ast``, never an import), so it runs anywhere
doppler itself doesn't build -- e.g. the docs CI job.
"""

from __future__ import annotations

import ast
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PKG_DIR = os.path.join(ROOT, "src", "doppler")

# Subpackages that are applications / entry points, not an importable API.
SKIP_TOP = {"cli", "specan", "tests", "benchmarks"}
# No-``__all__`` submodules whose public surface should still be covered,
# as (dotted name, path-relative-to-PKG_DIR).
EXTRA_MODULES = [
    ("doppler.wfm.compose", "wfm/compose.py"),
]


def all_list(path: str) -> list[str] | None:
    """The literal ``__all__`` list from a Python file, or None if absent."""
    try:
        with open(path, encoding="utf-8") as fh:
            tree = ast.parse(fh.read())
    except (OSError, SyntaxError):
        return None
    for node in tree.body:
        if (
            isinstance(node, ast.Assign)
            and any(
                isinstance(t, ast.Name) and t.id == "__all__"
                for t in node.targets
            )
            and isinstance(node.value, (ast.List, ast.Tuple))
        ):
            return [
                e.value
                for e in node.value.elts
                if isinstance(e, ast.Constant) and isinstance(e.value, str)
            ]
    return None


def public_defs(path: str) -> list[str]:
    """Top-level public class/def names defined in a module file."""
    with open(path, encoding="utf-8") as fh:
        tree = ast.parse(fh.read())
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
        allv = all_list(init)
        if allv:
            mods.append((f"doppler.{entry}", allv))
    for name, rel in EXTRA_MODULES:
        p = os.path.join(PKG_DIR, rel)
        if os.path.exists(p):
            mods.append((name, public_defs(p)))
    return mods
