#!/usr/bin/env python3
"""One source of truth for the just-makeit pin, and a gate that proves it.

doppler states the jm version in THREE places, and they must agree:

===========================================  ==================================
site                                         spelling
===========================================  ==================================
``just-makeit.toml`` ``[project] jm_version``  the SSOT -- what jm itself reads
``pyproject.toml`` dev group                   ``just-makeit==X.Y.Z``
``examples/downstream-jm/just-makeit.toml``    ``jm_version`` again
===========================================  ==================================

Nothing enforced that before this script. A bump is three hand edits, and a
missed one is **invisible**: verified by mutation on 2026-08-07 -- reverting
the downstream pin alone left ``make drift-check`` exiting **0**, with jm's
gh-183 skew notice reduced to one ``warning:`` line inside a wall of advisory
output. ``drift-check``'s own comment says the example "cannot silently
document a jm version doppler is not on", which was an intent, not a mechanism.

``scripts/check_version_strings.py`` does not cover this: it guards
*doppler's* release version against being hand-typed into docs -- a different
number entirely.

Usage
-----
::

    python scripts/gen_jm_pin.py --check   # exit 1 on any disagreement
    python scripts/gen_jm_pin.py --write   # rewrite the derived sites
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import tomllib

ROOT = Path(__file__).resolve().parent.parent

SSOT = ROOT / "just-makeit.toml"
PYPROJECT = ROOT / "pyproject.toml"
DOWNSTREAM = ROOT / "examples" / "downstream-jm" / "just-makeit.toml"

# Anchored to the start of a line so a version quoted in PROSE cannot match --
# just-makeit.toml carries commentary naming older jm versions, and a comment
# is history, not a claim about the present pin.
JM_VERSION_RE = re.compile(r'^(jm_version\s*=\s*")([^"]+)(")', re.MULTILINE)
# The dev-group requirement. `==` only: a range would make "the pinned version"
# unanswerable, which is the whole point of pinning it here.
DEP_RE = re.compile(r'^(\s*"just-makeit==)([^"]+)(",?\s*)$', re.MULTILINE)


def ssot_version() -> str:
    """Read the pin every other site must match."""
    with SSOT.open("rb") as fh:
        data = tomllib.load(fh)
    version = data.get("project", {}).get("jm_version")
    if not version:
        raise SystemExit(f"gen_jm_pin: no [project] jm_version in {SSOT.name}")
    return str(version)


def sites(want: str) -> list[tuple[Path, re.Pattern[str], str]]:
    """Every derived site: (path, pattern, the text it should hold)."""
    return [
        (PYPROJECT, DEP_RE, want),
        (DOWNSTREAM, JM_VERSION_RE, want),
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = ap.parse_args()

    want = ssot_version()
    bad: list[str] = []
    wrote: list[str] = []

    for path, pattern, expected in sites(want):
        text = path.read_text(encoding="utf-8")
        found = pattern.search(text)
        if not found:
            bad.append(f"{path.relative_to(ROOT)}: no just-makeit pin found")
            continue
        got = found.group(2)
        if got == expected:
            continue
        if args.write:
            path.write_text(
                pattern.sub(rf"\g<1>{expected}\g<3>", text, count=1),
                encoding="utf-8",
            )
            wrote.append(f"{path.relative_to(ROOT)}: {got} -> {expected}")
        else:
            bad.append(
                f"{path.relative_to(ROOT)}: has {got}, "
                f"{SSOT.name} says {expected}"
            )

    if bad:
        print("gen_jm_pin: the just-makeit pin disagrees across sites:")
        for line in bad:
            print(f"  {line}")
        print(f"\n  fix: python {Path(__file__).name} --write")
        return 1

    if args.write:
        for line in wrote:
            print(f"gen_jm_pin: {line}")
        print(f"gen_jm_pin: all sites pinned to {want}")
    else:
        print(f"gen_jm_pin: just-makeit pinned to {want} in all 3 sites")
    return 0


if __name__ == "__main__":
    sys.exit(main())
