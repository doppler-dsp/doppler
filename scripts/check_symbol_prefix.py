#!/usr/bin/env python3
"""Gate: every symbol the installed archives export carries the ``dp_`` prefix.

Two C libraries that each export ``fir_create`` cannot be linked into one
program, and the linker says so only at the consumer's build -- ours never
sees it. #1545 moved every symbol just-makeit derives onto ``dp_`` (``[project]
c_prefix = "dp"``, jm#1591); #1565 moved the rest -- 1090 exports on the day
this gate landed as a ratchet: hand-named cores (``wfm_*``, ``resamp_*``,
``hbdecim_*``, ``ccsds_*``) and vendored code linked in whole (cJSON, pffft,
pocketfft and nats.c, now ``dp__v_*`` via ``cmake/prefix_vendored.cmake``).

Absolute
--------
The ratchet reached zero with jm 0.92.2 and was deleted: there is no
allowlist. A new export is named ``dp_*`` -- ``dp__*`` for an internal one
that must cross files -- or it is ``static``. A list here would only be
somewhere for the next one to hide.

Where a symbol may live
-----------------------
The two archives the package installs, read by the same ``archives()`` as
``check_installed_headers.py`` (``DOPPLER_BUILD_DIR`` points it at another
build, which is how the gate's own test sabotages it). ``nm`` failing, or
reading no symbols at all, is a failure: absent output is not a pass.

Usage:  python3 scripts/check_symbol_prefix.py
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from check_installed_headers import archives

if TYPE_CHECKING:
    from pathlib import Path

PREFIX = "dp_"
# nm's global, defined kinds: text, data, bss, read-only, common, weak.
KINDS = frozenset("TDBRCWV")


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


def main() -> int:
    libs = archives()
    if not libs:
        print("check-symbol-prefix: no archive under build/ -- run make build")
        return 1
    syms = exported(libs)
    found = sorted(bare(syms))
    for s in found:
        print(f"  bare export: {s} -- name it {PREFIX}* or make it static")
    if found:
        print(f"check-symbol-prefix: FAIL ({len(found)} bare export(s))")
        return 1
    print(
        f"check-symbol-prefix: OK -- all {len(syms)} exported symbol(s) "
        f"carry {PREFIX}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
