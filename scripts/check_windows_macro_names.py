#!/usr/bin/env python3
"""Gate: no C identifier is a name the Windows SDK headers define away.

`<windows.h>` and the headers it pulls in define several plain words as
OBJECT-LIKE macros. ``minwindef.h`` defines ``near``, ``far``, ``NEAR``,
``FAR``, ``IN``, ``OUT`` and ``OPTIONAL`` as nothing and ``PASCAL`` /
``pascal`` as a calling convention; ``rpcndr.h`` defines ``small`` as
``char`` and ``hyper`` as ``__int64``; ``combaseapi.h`` defines
``interface`` as ``struct``; ``wingdi.h`` defines ``ERROR``,
``ABSOLUTE``, ``RELATIVE``, ``TRANSPARENT`` and ``OPAQUE`` as small
integers and ``winnt.h`` defines ``DELETE``. A C identifier with one of
those names is rewritten before the compiler sees it, so
``const size_t near[3]`` reads ``const size_t [3]`` and the file stops
building -- on the Windows legs only, where a Linux or macOS build and
every local ``make test`` stay green.

It happened twice. A test's ``enum { IN = 32 }`` broke
``test_framer_core`` on PR #1975 (2026-10-09), and a ``near[3]`` broke
``test_async_dsss_pool_core`` on PR #3071 (2026-11-15). Both were
caught only by a Windows CI leg, after a push.

The rule
--------
None of the names in :data:`WINDOWS_MACROS` appears as a whole token in a
tracked ``.c`` or ``.h`` outside ``vendor/``, with comments and string
and character literals blanked first (prose and messages may say "near").
The ban is unconditional, not limited to translation units that include
``<windows.h>`` today: whether one does is a fact about a transitive
include chain, and it changes when a header gains an include.

The existing uses are a ratchet
-------------------------------
When the gate landed, ``near``, ``far`` and ``small`` sat in five tests
that build on Windows only because nothing they include reaches
``<windows.h>``. Each such (file, name) pair is a line of
``scripts/.windows-macro-names-exempt``. A use is allowed only where its
pair is listed, and a listed pair the file no longer uses fails too, so
the list can only shrink: rename the identifier, then delete its line.

Usage:  python3 scripts/check_windows_macro_names.py [--root DIR]
Exit 0 when every use is listed and every listed pair is still used.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

from _c_source import strip_comments
from _gitbase import in_git_repo

ROOT = Path(__file__).resolve().parent.parent
SKIP_PARTS = {"build", "vendor", ".venv", "node_modules", ".git"}
EXEMPT = "scripts/.windows-macro-names-exempt"

#: Object-like macros of the Windows SDK that a C identifier could collide
#: with, and the header that defines each. Function-like macros (``min``,
#: ``max``) are not here: they fire only before a ``(``.
WINDOWS_MACROS = {
    "IN": "minwindef.h",
    "OUT": "minwindef.h",
    "OPTIONAL": "minwindef.h",
    "near": "minwindef.h",
    "far": "minwindef.h",
    "NEAR": "minwindef.h",
    "FAR": "minwindef.h",
    "pascal": "minwindef.h",
    "PASCAL": "minwindef.h",
    "small": "rpcndr.h",
    "hyper": "rpcndr.h",
    "interface": "combaseapi.h",
    "ERROR": "wingdi.h",
    "ABSOLUTE": "wingdi.h",
    "RELATIVE": "wingdi.h",
    "TRANSPARENT": "wingdi.h",
    "OPAQUE": "wingdi.h",
    "DELETE": "winnt.h",
}
# Not here on purpose: TRUE, FALSE, BOOL, DWORD, WINAPI, CALLBACK. The
# platform shims (dp_thread.h, dp_once.c, ...) use them as Windows means
# them, inside _WIN32 blocks.

TOKEN = re.compile(r"\b(" + "|".join(WINDOWS_MACROS) + r")\b")
LITERAL = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'')
INCLUDE = re.compile(r"\s*#\s*include\b")


def _files(root: Path) -> list[Path]:
    """Tracked C sources and headers, or every one under a synthetic tree.

    A gate's test harness seeds a directory that is not a repository, so
    the walk stands in for `git ls-files` there; it skips the same
    `vendor/` and the build trees a real checkout would not track.
    """
    if in_git_repo(root):
        out = subprocess.run(
            ["git", "ls-files", "-z", "--", "*.c", "*.h"],
            cwd=root,
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        paths = [root / r for r in out.split("\0") if r]
    else:
        paths = [*root.rglob("*.c"), *root.rglob("*.h")]
    return sorted(
        p
        for p in paths
        if not SKIP_PARTS.intersection(p.relative_to(root).parts)
    )


def scan(text: str) -> list[tuple[int, str]]:
    """Return ``(line, name)`` for each listed name used as a C token.

    >>> scan("const size_t near[3];")
    [(1, 'near')]
    >>> scan('/* near */ x = "far"; c = \\'IN\\';')
    []
    >>> scan("int nearest, farther, INPUT;")
    []
    >>> scan("#include <far/x.h>")
    []
    """
    code = LITERAL.sub('""', strip_comments(text))
    return [
        (i, m.group(1))
        for i, line in enumerate(code.splitlines(), 1)
        if not INCLUDE.match(line)  # a <header-name> is never expanded
        for m in TOKEN.finditer(line)
    ]


def _exempt(root: Path) -> set[tuple[str, str]]:
    """The ratchet's (file, name) pairs; ``#`` starts a comment line."""
    path = root / EXEMPT
    if not path.exists():
        return set()
    pairs = set()
    lines = path.read_text(encoding="utf-8").splitlines()
    for n, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 2 or fields[1] not in WINDOWS_MACROS:
            sys.exit(
                f'{EXEMPT}:{n}: expected "<file> <name>" with a name from '
                f"WINDOWS_MACROS, got {line!r}"
            )
        pairs.add((fields[0], fields[1]))
    return pairs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args().root.resolve()
    exempt = _exempt(root)
    seen: set[tuple[str, str]] = set()
    bad = 0
    for p in _files(root):
        rel = p.relative_to(root).as_posix()
        text = p.read_text(encoding="utf-8", errors="replace")
        for line, name in scan(text):
            if (rel, name) in exempt:
                seen.add((rel, name))
                continue
            print(
                f"{rel}:{line}: '{name}' is a macro in "
                f"{WINDOWS_MACROS[name]}; rename the identifier"
            )
            bad += 1
    for rel, name in sorted(exempt - seen):
        print(
            f"{EXEMPT}: '{rel} {name}' is no longer used; delete the line "
            "(the list only shrinks)"
        )
        bad += 1
    if bad:
        print(
            f"\n{bad} problem(s): a name <windows.h> defines away breaks "
            "only the Windows build (scripts/check_windows_macro_names.py)."
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
