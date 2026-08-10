#!/usr/bin/env python3
"""Gate: the double -> phase-word conversion has ONE home.

`native/inc/nco/nco_core.h` states this as a structural rule, not a
stylistic one, and explains why:

    Undefined behaviour can enter at exactly one place: a `double` whose
    truncated value the integer type cannot represent (6.3.1.4). That makes
    confining the conversion a STRUCTURAL rule rather than a stylistic one.
    A second site anywhere forfeits the guarantee no matter how careful this
    function is.

It also records that this has already gone wrong once -- "duplicated copies
of this exact formula have already drifted once (one truncated while a
sibling copy rounded)" -- which is exactly the failure a rule with no gate
behind it produces. This is the gate.

The signature of a private conversion is the 2^32 scaling constant appearing
outside the sanctioned file. That constant also has legitimate uses in the
INVERSE direction (phase word -> double, which cannot trap), so this script
does not try to classify: it enumerates every occurrence and compares against
a checked-in allowlist that carries a reason per entry.

**The allowlist is a RATCHET. It may only shrink.** A new occurrence fails
the gate. Removing one means deleting its line here, which the failure
message tells you how to do.

Usage:  python3 scripts/check_phase_conversion_sites.py
Exit 0 when the set of occurrences matches the allowlist exactly.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALLOW = Path(__file__).parent / ".phase-conversion-allow"

# The sanctioned home. Occurrences here are the point of the rule.
SANCTIONED = "native/inc/nco/nco_core.h"

# Where a private copy would do damage: the library itself. Tests and
# validation harnesses may scale by 2^32 freely -- they are oracles, and an
# oracle that could not express the constant could not check the conversion.
SCAN_DIRS = ("native/inc", "native/src")

CONST = re.compile(r"4294967296")


def occurrences() -> list[tuple[str, int, str]]:
    """Every 2^32 occurrence in library C, outside the sanctioned file."""
    found: list[tuple[str, int, str]] = []
    for d in SCAN_DIRS:
        for path in sorted((ROOT / d).rglob("*")):
            if path.suffix not in (".c", ".h"):
                continue
            rel = path.relative_to(ROOT).as_posix()
            if rel == SANCTIONED:
                continue
            for n, line in enumerate(
                path.read_text(errors="replace").splitlines(), 1
            ):
                if CONST.search(line):
                    found.append((rel, n, line.strip()))
    return found


def load_allow() -> dict[str, str]:
    """Allowed `file:snippet` keys mapped to their stated reason."""
    allowed: dict[str, str] = {}
    if not ALLOW.exists():
        return allowed
    for raw in ALLOW.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key, _, reason = line.partition("|")
        allowed[key.strip()] = reason.strip()
    return allowed


def key_for(rel: str, line: str) -> str:
    """Line-number-free identity, so unrelated edits don't churn the list."""
    return f"{rel}::{' '.join(line.split())}"


def main() -> int:
    found = occurrences()
    allowed = load_allow()

    seen: set[str] = set()
    new: list[tuple[str, int, str]] = []
    for rel, n, line in found:
        k = key_for(rel, line)
        seen.add(k)
        if k not in allowed:
            new.append((rel, n, line))

    stale = sorted(set(allowed) - seen)

    if new:
        print(
            "FAIL: a new double -> phase-word conversion site appeared.\n"
            "\n"
            "nco_core.h confines this conversion to one place because a\n"
            "second site forfeits the C99 6.3.1.4 guarantee no matter how\n"
            "careful the first one is. Route the value through\n"
            "nco_norm_freq_to_inc() or nco_norm_phase_to_word() instead.\n"
        )
        for rel, n, line in new:
            print(f"  {rel}:{n}\n      {line}")
        print(
            "\nIf this really is an inverse conversion (phase word ->\n"
            "double, which cannot trap) add it to\n"
            f"  {ALLOW.relative_to(ROOT)}\n"
            "with a reason, in the form:\n"
            f"  {key_for(new[0][0], new[0][2])} | why this is safe"
        )
        return 1

    if stale:
        print(
            "FAIL: the allowlist is a ratchet and has gone stale -- these\n"
            "entries no longer match anything. Delete them; the rule just\n"
            "got stricter and that is the direction it is allowed to move.\n"
        )
        for k in stale:
            print(f"  {k}")
        return 1

    print(
        f"phase-conversion gate: OK -- {len(found)} occurrence(s), all "
        f"allowlisted, sanctioned home is {SANCTIONED}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
