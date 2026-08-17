#!/usr/bin/env python3
"""Gate: a retired identifier stays retired.

A rename is not finished when the build goes green -- it is finished when the
old name appears nowhere. Those two are not the same event, and the gap
between them is where this repository has already been bitten:
`fec_` -> `ccsds_tm_` (doppler#828) renamed 30 symbols across 31 files, went
green, passed every gate, and left `FEC_CCSDS_ASM` and `FEC_CONV_K` behind --
in a public header, under the very prefix the rename existed to remove. The
PR body asserted one of them was already gone. A compiler cannot notice: both
names still compiled, because both still existed.

So the rule is data rather than vigilance. `scripts/.retired-names` lists the
patterns that must not reappear, one per line, with the replacement:

    <regex><TAB><what to use instead>

Every hand-written file in the scanned trees is checked. Generated trees are
excluded because they are regenerated from the sources that are checked --
gating both would report the same defect twice and make the fix order matter.

**This list only GROWS, and each row only ever goes to zero occurrences.**
Retiring a name means adding its row here in the same commit that renames it,
which is what makes the next rename's stragglers a failure rather than a
discovery months later.

Usage:  python3 scripts/check_retired_names.py
Exit 0 when no retired name occurs anywhere in the scanned trees.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RETIRED = Path(__file__).parent / ".retired-names"

# Hand-written trees. `docs/c-api` and `build` are generated from `native/`,
# and `.git` is not source.
SCAN_DIRS = (
    "native",
    "examples",
    "src/doppler",
    "scripts",
    "changelog.d",
    "docs",
)
SCAN_FILES = ("Makefile", "CMakeLists.txt", "just-makeit.toml", "mkdocs.yml")
SKIP_PARTS = frozenset({"build", "docs/c-api", "__pycache__", ".git"})
SUFFIXES = frozenset(
    {".c", ".h", ".py", ".md", ".txt", ".toml", ".yml", ".yaml", ".sh", ".pyi"}
)


def _rows() -> list[tuple[re.Pattern[str], str, str]]:
    """Parse `.retired-names` into (compiled, raw pattern, replacement)."""
    out: list[tuple[re.Pattern[str], str, str]] = []
    for n, line in enumerate(RETIRED.read_text().splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "\t" not in line:
            print(
                f"{RETIRED.name}:{n}: expected '<regex>\\t<replacement>'",
                file=sys.stderr,
            )
            sys.exit(2)
        pat, repl = line.split("\t", 1)
        out.append((re.compile(pat), pat, repl.strip()))
    return out


def _files() -> list[Path]:
    seen: list[Path] = []
    for name in SCAN_FILES:
        p = ROOT / name
        if p.is_file():
            seen.append(p)
    for d in SCAN_DIRS:
        base = ROOT / d
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if not p.is_file() or p.suffix not in SUFFIXES:
                continue
            rel = p.relative_to(ROOT).as_posix()
            if any(part in SKIP_PARTS for part in p.parts):
                continue
            if any(rel.startswith(s) for s in SKIP_PARTS):
                continue
            seen.append(p)
    return seen


def main() -> int:
    rows = _rows()
    if not rows:
        print("retired-names: no retired names declared")
        return 0

    # This file necessarily contains every retired name, in the prose that
    # explains why. So does its data file.
    exempt = {RETIRED.resolve(), Path(__file__).resolve()}

    hits: list[str] = []
    for path in _files():
        if path.resolve() in exempt:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for compiled, pat, repl in rows:
            for n, line in enumerate(text.splitlines(), 1):
                if compiled.search(line):
                    rel = path.relative_to(ROOT).as_posix()
                    hits.append(f"  {rel}:{n}: {pat} -> use {repl}")

    if hits:
        print(f"retired-names: {len(hits)} occurrence(s) of a retired name.")
        print("\n".join(sorted(hits)))
        print(
            "\nA rename is finished when the old name appears NOWHERE. Fix"
            " the\noccurrences above; do not add a row to"
            f" {RETIRED.name} for them."
        )
        return 1

    print(f"retired-names: OK — {len(rows)} retired pattern(s), 0 occurrences")
    return 0


if __name__ == "__main__":
    sys.exit(main())
