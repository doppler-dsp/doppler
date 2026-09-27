#!/usr/bin/env python3
"""Gate: every CMake script run with ``cmake -P`` declares its policy level.

A ``cmake -P`` script inherits no policies from the project that calls it:
unless it says ``cmake_minimum_required(...)`` itself, every policy is OLD.
On a new CMake that is invisible -- CMake 4 made the old behaviour of several
policies unavailable, so a script "works" locally -- and on the CMake a CI
image or a distro ships it is a hard error. ``prefix_vendored.cmake`` used
``if(x IN_LIST y)`` (CMP0057), passed on a CMake 4.4 dev box, and failed every
Linux build in CI on its first push (#1578).

The scripts are discovered, not listed: every ``-P <path>.cmake`` named by a
CMakeLists.txt, a ``.cmake`` file or the Makefile, resolved against the repo
root (``${CMAKE_SOURCE_DIR}`` and ``$(CURDIR)`` are the root).

Usage:  python3 scripts/check_cmake_script_policy.py [ROOT]

ROOT (default: the repo) lets the gate be sabotaged on a scratch tree.
Exit 0 when every such script declares cmake_minimum_required.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent.parent
SKIP = {"build", "vendor", ".venv", ".git", "node_modules"}

#: `-P ${CMAKE_SOURCE_DIR}/cmake/x.cmake`, `-P cmake/x.cmake`, ...
P_ARG = re.compile(
    r"-P\s+(?:\$\{CMAKE_(?:CURRENT_)?SOURCE_DIR\}/|\$\(CURDIR\)/)?"
    r"([\w./-]+\.cmake)"
)
DECLARES = re.compile(r"^\s*cmake_minimum_required\s*\(", re.I | re.M)


def callers(root: Path):
    """Every file that may invoke ``cmake -P``."""
    yield root / "Makefile"
    for pat in ("CMakeLists.txt", "*.cmake", "*.mk"):
        for p in root.rglob(pat):
            if not SKIP & set(p.relative_to(root).parts):
                yield p


def main() -> int:
    scripts: dict[Path, str] = {}
    for f in callers(ROOT):
        if not f.is_file():
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in P_ARG.finditer(text):
            scripts.setdefault(ROOT / m.group(1), str(f.relative_to(ROOT)))
    if not scripts:
        print(
            "check_cmake_script_policy: found no `cmake -P` script -- the"
            " discovery is broken, not the tree clean"
        )
        return 1
    bad = []
    for path, caller in sorted(scripts.items()):
        rel = path.relative_to(ROOT)
        if not path.is_file():
            bad.append(f"  {rel}: named by {caller} with -P, does not exist")
        elif not DECLARES.search(path.read_text(encoding="utf-8")):
            bad.append(
                f"  {rel}: run with -P (by {caller}) but never says"
                " cmake_minimum_required(...) -- every policy is OLD there"
            )
    if bad:
        print("check_cmake_script_policy: FAIL")
        print("\n".join(bad))
        return 1
    print(
        f"check_cmake_script_policy: OK -- {len(scripts)} `cmake -P`"
        " script(s), each declares its policy level"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
