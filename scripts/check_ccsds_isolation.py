#!/usr/bin/env python3
"""No component outside ``ccsds_tm`` includes a ``ccsds_tm`` header.

Why this exists
---------------
Framing decomposed the way every other standard-specific thing in doppler
already had: a general primitive plus a configuration. ``wfm/wfm_frame.h``
owns fields, stages and covers and knows nothing about CCSDS; ``ccsds_tm``
configures it. The header says so itself, and says why it must stay that way::

    `ccsds_tm` must depend on this file to describe a CADU, so this file must
    not call `ccsds_tm`'s kernels, or the two components form a cycle.

That is a property of the include graph, and until now nothing measured it.
The design page proposed exactly this gate and it was never implemented, so
the layering was asserted in prose while four components quietly reached into
``ccsds_tm`` anyway — the largest of them being the Python object, which
hard-codes the CCSDS ops table and thereby makes CCSDS *the* implementation
rather than *an* example. See ``docs/design/frame-description.md``.

A ratchet, not a list
---------------------
The four known sites are named below and the gate fails in **both**
directions:

* a file that reaches into ``ccsds_tm`` and is **not** named here — the
  breakage grew, which is what a ratchet exists to stop;
* a name here that **no longer** reaches into ``ccsds_tm`` — the site was cut
  and the list was not, so the allowlist has started describing a tree that no
  longer exists.

The second direction is the one that keeps this honest. An allowlist that only
ever fails upward rots into a permanent exemption nobody rereads; one that
also fails when it is too large can only shrink.

Scope is **components**: ``native/inc`` and ``native/src``. A test, a benchmark
or a validation harness that includes ``ccsds_tm`` is exercising it, not
depending on it, and those trees are deliberately not scanned.

Usage: check_ccsds_isolation.py [ROOT...]
         no arguments: scan native/inc and native/src
         with roots:   scan those instead (this is how the gate's own test
                       drives it, over a seeded tree rather than the repo)
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

#: Sites that reach into `ccsds_tm` today. Each one is a cut to make, with an
#: issue behind it -- not a permanent exemption. This list may only shrink.
ALLOWED = {
    "native/src/frame/frame_core.c",
    "native/src/wfm/wfm_synth_bridge.c",
    "native/src/wfm/ccsds_asm_bits.c",
    "native/src/burst_demod/burst_demod_core.c",
}

DEFAULT_ROOTS = ("native/inc", "native/src")

#: `#include "ccsds_tm/…"`. Angle-bracket form too: the component is included
#: by quoted path everywhere today, but a gate that only saw one spelling
#: would be one `<>` away from reporting a clean tree.
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]ccsds_tm/', re.MULTILINE)

SUFFIXES = (".c", ".h")


def scan(roots: list[Path], base: Path) -> tuple[set[str], int]:
    """Return the violating paths (relative to *base*) and the file count."""
    found: set[str] = set()
    seen = 0
    for root in roots:
        for path in sorted(root.rglob("*")):
            if path.suffix not in SUFFIXES or not path.is_file():
                continue
            seen += 1
            rel = path.resolve().relative_to(base).as_posix()
            # The component itself is where CCSDS belongs.
            if "/ccsds_tm/" in f"/{rel}":
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            if INCLUDE.search(text):
                found.add(rel)
    return found, seen


def main(argv: list[str]) -> int:
    # Paths are reported relative to the working directory, so a seeded tree
    # run with `cwd=` set reads exactly like the real one and the allowlist
    # means the same thing in both.
    base = Path.cwd().resolve()
    roots = [Path(a) for a in argv[1:]] or [Path(r) for r in DEFAULT_ROOTS]

    present = [r for r in roots if r.is_dir()]
    if not present:
        print("check_ccsds_isolation: no root to scan —", end=" ")
        print("a scan that looked at nothing has not passed.")
        return 1

    found, seen = scan(present, base)

    if seen == 0:
        print("check_ccsds_isolation: scanned 0 files — a scan that matches")
        print("  nothing has not passed, it has not run.")
        return 1

    new = sorted(found - ALLOWED)
    stale = sorted(ALLOWED - found)

    if not new and not stale:
        print(
            "check_ccsds_isolation: OK — "
            f"{seen} file(s), {len(ALLOWED)} known site(s), none new"
        )
        return 0

    print("check_ccsds_isolation: FAIL")
    if new:
        print()
        print("  These reach into `ccsds_tm` and are not on the list:")
        for p in new:
            print(f"    {p}")
        print()
        print("  A component outside `ccsds_tm` must not include its headers.")
        print("  CCSDS is a configuration of the frame description, not its")
        print("  shape — see docs/design/frame-description.md. If this is a")
        print("  cut you are making rather than one you are adding, the")
        print("  entry comes OFF the list in the same commit.")
    if stale:
        print()
        print("  These are on the list but no longer reach into `ccsds_tm`:")
        for p in stale:
            print(f"    {p}")
        print()
        print("  The site was cut and the list was not. Delete the entry in")
        print("  scripts/check_ccsds_isolation.py — the list may only shrink,")
        print("  and one that is too large is an exemption nobody rereads.")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
