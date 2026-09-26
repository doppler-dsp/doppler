#!/usr/bin/env python3
"""Gate: every symbol the installed archives export carries the ``dp_`` prefix.

Two C libraries that each export ``fir_create`` cannot be linked into one
program, and the linker says so only at the consumer's build -- ours never
sees it. #1545 moved every symbol just-makeit derives onto ``dp_`` (``[project]
c_prefix = "dp"``, jm#1591), and that is the part a generator can reach. It
cannot reach a name we wrote ourselves, and measured on the day it landed the
two archives still exported 1092 of those: hand-named cores (``wfm_*``,
``resamp_*``, ``hbdecim_*``, ``ccsds_*``) and vendored code linked in whole
(cJSON, pffft, pocketfft, nats.c).

Ratcheted, not absolute
-----------------------
``scripts/.symbol-prefix-ratchet`` lists each bare export that predates this
gate, one per line. **It may only shrink.** A new bare export is a failure --
name it ``dp_*`` or keep it ``static`` -- and so is a listed name the archives
no longer export: the entry went stale, delete the line. Burn-down: #1565.

Where a symbol may live
-----------------------
The two archives the package installs, read by the same ``archives()`` as
``check_installed_headers.py``. ``nm`` failing, or reading no symbols at all,
is a failure: absent output is not a pass.

Usage:  python3 scripts/check_symbol_prefix.py [--update-ratchet]
                                               [--ratchet=PATH]

``--ratchet`` exists for the gate's own test, which seeds archives and a
list in a scratch directory so the gate can be sabotaged.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from check_installed_headers import archives

ROOT = Path(__file__).resolve().parent.parent
RATCHET = Path(__file__).parent / ".symbol-prefix-ratchet"
PREFIX = "dp_"
# nm's global, defined kinds: text, data, bss, read-only, common, weak.
KINDS = frozenset("TDBRCWV")

HEADER = """\
# Ratchet for scripts/check_symbol_prefix.py: exported symbols without dp_.
#
# THIS LIST MAY ONLY SHRINK. A new export is named dp_* or made static; an
# entry the archives no longer export is stale and fails until deleted.
# Regenerate after a burn-down with:
#     python3 scripts/check_symbol_prefix.py --update-ratchet
# Burn-down: doppler#1565.
"""


def exported(libs: list[Path]) -> set[str]:
    """Global defined symbols of ``libs``; raise if nm fails or finds none."""
    out: set[str] = set()
    for lib in libs:
        proc = subprocess.run(
            ["nm", "-g", "--defined-only", str(lib)],
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            raise SystemExit(f"check-symbol-prefix: nm {lib}: {proc.stderr}")
        for line in proc.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[-2] in KINDS:
                out.add(parts[-1])
    if not out:
        raise SystemExit("check-symbol-prefix: no symbols read -- not a pass")
    return out


def bare(symbols: set[str]) -> set[str]:
    """Exports outside the dp_ namespace (compiler-reserved ``_*`` aside)."""
    return {s for s in symbols if not s.startswith((PREFIX, "_"))}


def main(argv: list[str]) -> int:
    libs = archives()
    if not libs:
        print("check-symbol-prefix: no archive under build/ -- run make build")
        return 1
    ratchet = RATCHET
    for a in argv:
        if a.startswith("--ratchet="):
            ratchet = Path(a.split("=", 1)[1])
    now = bare(exported(libs))
    if "--update-ratchet" in argv:
        ratchet.write_text(HEADER + "".join(f"{s}\n" for s in sorted(now)))
        print(f"check-symbol-prefix: ratchet rewritten, {len(now)} entries")
        return 0
    listed = {
        ln.strip()
        for ln in ratchet.read_text(encoding="utf-8").splitlines()
        if ln.strip() and not ln.startswith("#")
    }
    new, stale = sorted(now - listed), sorted(listed - now)
    for s in new:
        print(f"  NEW bare export: {s} -- name it {PREFIX}* or make it static")
    for s in stale:
        print(f"  STALE ratchet entry: {s} -- no longer exported, delete it")
    if new or stale:
        print(
            f"check-symbol-prefix: FAIL ({len(new)} new, {len(stale)} stale)"
        )
        return 1
    print(f"check-symbol-prefix: OK -- {len(now)} ratcheted bare exports")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
