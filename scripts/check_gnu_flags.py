#!/usr/bin/env python3
"""Gate: no target_compile_options() names a GCC-style flag directly.

clang-cl reads MSVC's option syntax. Handed a GCC spelling it does not
share -- `-O3`, `-funroll-loops`, `-ffast-math`, `-ffp-contract=off`,
`-mfma` -- it prints `unknown argument ignored` and builds without it. That
is worse than an error: the binary still builds and still runs, just with the
default optimisation and float contraction, so a validation that pinned
`-ffp-contract=off` measures something else on Windows and nobody is told.
All 35 such calls in native/validation/ were written that way (doppler#1360),
because every one of them works on the platform doing the writing.

The fix is one CMake function in the root, `dp_gnu_compile_options(target
scope flags...)`, which passes flags through verbatim for gcc and clang and
behind `/clang:` for clang-cl. This gate fails any target_compile_options()
that names a GCC-only flag instead of calling it.

A flag is GCC-only here when it starts `-f` or `-m`, or is `-O3`/`-Ofast`
(MSVC has neither level; `-O1`/`-O2` are accepted by both drivers and pass).
`/clang:-f...` and generator expressions are not direct, and are left
alone. add_compile_options() is NOT scanned: the root's global flags already
branch on the driver, and a guard is not something a line scanner can see.

Usage:  python3 scripts/check_gnu_flags.py [--root DIR]
Exit 0 when no target_compile_options() names a GCC-only flag.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from check_bare_libm import ROOT, _files, _scan

CALL = re.compile(r"\btarget_compile_options\s*\(", re.IGNORECASE)
# A whole item starting -f / -m, or an optimisation level MSVC lacks. The
# look-behind is the same "stands alone" test as the libm gate, so
# `/clang:-O3` and `$<...:-ffast-math>` are not matches.
GNU_FLAG = re.compile(
    r"(?<![\w$<>{}:./@-])-(?:[fm][\w=+.-]*|O(?:3|fast))(?![\w$<>{}:./@-])"
)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args().root.resolve()

    found: list[tuple[str, int, str]] = []
    for path in _files(root):
        text = path.read_text(errors="replace")
        lines = text.split("\n")
        for i, _ in _scan(text, CALL, GNU_FLAG):
            found.append(
                (path.relative_to(root).as_posix(), i + 1, lines[i].strip())
            )

    if not found:
        print("gnu-flags: no target_compile_options() names a GCC-only flag")
        return 0
    print("gnu-flags: target_compile_options() names a GCC-only flag --")
    for rel, n, line in sorted(set(found)):
        print(f"  {rel}:{n}: {line}")
    print(
        "  clang-cl ignores these with a warning and builds without them.\n"
        "  Call dp_gnu_compile_options(<target> <scope> <flags>...) instead:\n"
        "  it passes them verbatim to gcc/clang and as /clang:<flag> to\n"
        "  clang-cl."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
