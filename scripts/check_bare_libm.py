#!/usr/bin/env python3
"""Gate: no CMake link line names libm as a bare `m`.

On POSIX libm is a separate library and a bare `m` in `target_link_libraries`
finds it. On Windows the math functions live in the UCRT itself, there is no
libm, and `m` becomes a request for `m.lib` -- which does not exist, so the
link dies after every object has compiled. doppler's C library first built on
Windows only once jm resolved libm by path (gh-1305, jm 0.76.x), and at that
point ~95 hand-written link lines, plus eight `extra_link_libs = ["m"]`
entries in the manifest, still named it bare (doppler#1364).

The fix is one variable, resolved once in the root CMakeLists:

    find_library(DP_MATH_LIBRARY m)          # a path on POSIX, "" on Windows

and jm's generated files carry their own `${JM_MATH_LIBRARY}`. A bare `m` is
therefore always a regression, and it is an easy one to write: it is what
every CMake tutorial shows, and it works on the platform doing the writing.
So the gate has no allowlist -- every site was converted when it landed, and
the first new one fails.

Only `target_link_libraries(...)` arguments are read, comments stripped, and
`m` must stand alone as an item: `mvec`, `$<...m...>` and prose about libm
are not links. A bare `m` in a jm-GENERATED file means the manifest declares
it (`extra_link_libs = ["m"]`) -- fix it there, not in the file, because
`jm apply` re-renders the file from the manifest.

Usage:  python3 scripts/check_bare_libm.py [--root DIR] [--fix]
Exit 0 when no link line names a bare `m`.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIX = "${DP_MATH_LIBRARY}"
# Build output and third-party trees are not ours to police.
SKIP_PARTS = {"build", "build-cov", "vendor", ".venv", "node_modules", ".git"}

CALL = re.compile(r"\btarget_link_libraries\s*\(", re.IGNORECASE)
# `m` as a whole link item: not part of a longer word, not inside a
# generator expression or a variable reference.
BARE_M = re.compile(r"(?<![\w$<>{}:./@-])m(?![\w$<>{}:./@-])")


PROJECT = re.compile(r"^\s*project\s*\(", re.MULTILINE)


def _owning_project(path: Path, root: Path) -> Path:
    """The nearest enclosing directory whose CMakeLists declares project().

    DERIVED rather than listed, because this is the fact the fix depends on:
    `${DP_MATH_LIBRARY}` is resolved in doppler's root CMakeLists, so it
    exists only inside that project. A separate project in the tree -- the
    downstream-jm showcase, an example-projects/ build -- never sees it, and
    rewriting its `m` would silently drop libm on Linux. A hand list of such
    directories would go stale the first time someone added one.
    """
    d = path.parent
    while True:
        cml = d / "CMakeLists.txt"
        if cml.exists() and PROJECT.search(cml.read_text(errors="replace")):
            return d
        if d == root or d.parent == d:
            return root
        d = d.parent


def _files(root: Path) -> list[Path]:
    out = []
    for p in sorted(root.rglob("*")):
        if p.name != "CMakeLists.txt" and p.suffix != ".cmake":
            continue
        if SKIP_PARTS & set(p.relative_to(root).parts):
            continue
        if _owning_project(p, root) != root:
            continue  # a separate project: DP_MATH_LIBRARY is not in scope
        out.append(p)
    return out


def _scan(text: str) -> list[tuple[int, int]]:
    """(line_index, column) of every bare `m` inside a link call."""
    lines = text.split("\n")
    hits: list[tuple[int, int]] = []
    depth = 0
    for i, line in enumerate(lines):
        code = line.split("#", 1)[0]
        col = 0
        if depth == 0:
            m = CALL.search(code)
            if not m:
                continue
            depth, col = 1, m.end()
        # Walk this line's code, tracking parens, and test only the spans
        # that are inside the call.
        start = col
        j = col
        while j < len(code) and depth > 0:
            if code[j] == "(":
                depth += 1
            elif code[j] == ")":
                depth -= 1
            j += 1
        span = code[start:j]
        for bm in BARE_M.finditer(span):
            hits.append((i, start + bm.start()))
    return hits


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument(
        "--fix", action="store_true", help=f"rewrite each hit as {FIX}"
    )
    args = ap.parse_args()
    root = args.root.resolve()

    found: list[tuple[str, int, str]] = []
    fixed = 0
    for path in _files(root):
        text = path.read_text(errors="replace")
        hits = _scan(text)
        if not hits:
            continue
        rel = path.relative_to(root).as_posix()
        lines = text.split("\n")
        if args.fix:
            # Right to left within a line so earlier columns stay valid.
            for i, col in sorted(hits, reverse=True):
                ln = lines[i]
                lines[i] = ln[:col] + FIX + ln[col + 1 :]
                fixed += 1
            path.write_text("\n".join(lines))
        else:
            for i, _ in hits:
                found.append((rel, i + 1, lines[i].strip()))

    if args.fix:
        print(f"bare-libm: rewrote {fixed} bare `m` link item(s) as {FIX}")
        return 0
    if not found:
        print("bare-libm: no link line names a bare `m`")
        return 0
    print("bare-libm: a link line names libm as a bare `m` --")
    for rel, n, line in found:
        print(f"  {rel}:{n}: {line}")
    print(
        "  On Windows libm is part of the UCRT and `m` means `m.lib`, which\n"
        "  does not exist, so the link fails after everything compiled. Use\n"
        f"  {FIX} (resolved once in the root CMakeLists: a path on POSIX,\n"
        '  empty on Windows). In a jm-generated file, remove `"m"` from the\n'
        "  manifest's extra_link_libs instead -- `jm apply` re-renders it."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
