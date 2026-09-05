#!/usr/bin/env python3
"""`wfm/wfm_frame.h` stays free of CCSDS, because a cycle would be silent.

Why this exists
---------------
The general frame primitive states the rule about itself, and states the
consequence of breaking it::

    `ccsds_tm` must depend on this file to describe a CADU, so this file must
    not call `ccsds_tm`'s kernels, or the two components form a cycle.

`ccsds_tm/CMakeLists.txt` repeats it from the other side. Nothing measured it,
so the layering the design rests on was prose.

What this does NOT check, and why
---------------------------------
An earlier version of this gate scanned every component and allowlisted the
four that include a `ccsds_tm` header. That was a broader rule than the one
above, inherited from a design-page proposal written before the code, and it
was wrong: `frame -> ccsds_tm -> wfm_frame` is **acyclic and deliberate**.
`objects/frame.toml` says so — `ccsds_tm` has no Python binding and is not
getting one, so `frame` is where a caller meets the outer code, the randomiser
and the inner code. A ratchet over a rule that should never reach zero is a
slow push toward a refactor nobody wants.

So the check is exactly the header's claim: the general primitive knows
nothing about the standard. A consumer composing the two is the design
working, not a violation of it.

What a cycle would cost
-----------------------
Not a link error, necessarily — a component that calls back into its own
dependent is how a build starts depending on link order, and how "the general
layer" quietly acquires a standard's defaults. The failure is architectural
and arrives late, which is why it is worth a gate that is cheap and exact.

Usage: check_ccsds_isolation.py [FILE...]
         no arguments: check the general primitive's own files
         with files:   check those instead (this is how the gate's own test
                       drives it, over a seeded tree)
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

#: The general primitive. These are the files whose own documentation says
#: they know nothing about CCSDS, and the only files this gate governs.
GUARDED = (
    "native/inc/wfm/wfm_frame.h",
    "native/src/wfm/wfm_frame.c",
)

#: `#include "ccsds_tm/…"`. Angle-bracket form too: the component is included
#: by quoted path everywhere today, but a gate that only saw one spelling
#: would be one `<>` away from reporting a clean tree.
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]ccsds_tm/', re.MULTILINE)

#: The kernels the primitive must not call, even without including a header —
#: a forward declaration reaches them just as well.
SYMBOL = re.compile(r"\bccsds_tm_[a-z_]+\s*\(")


def check(paths: list[Path]) -> tuple[list[str], int]:
    """Return the findings and the number of files actually read."""
    out: list[str] = []
    seen = 0
    for path in paths:
        if not path.is_file():
            out.append(
                f"{path}: missing — the gate cannot vouch for a file "
                "it did not read"
            )
            continue
        seen += 1
        text = path.read_text(encoding="utf-8", errors="replace")
        # Comments name CCSDS constantly and correctly: the header explains
        # the layering by naming the component on the other side of it. Only
        # code counts, so strip block comments before looking.
        code = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        code = re.sub(r"//[^\n]*", "", code)
        if INCLUDE.search(code):
            out.append(f"{path}: includes a ccsds_tm header")
        for m in SYMBOL.finditer(code):
            out.append(f"{path}: calls {m.group(0)[:-1].strip()}()")
    return out, seen


def main(argv: list[str]) -> int:
    paths = [Path(a) for a in argv[1:]] or [Path(p) for p in GUARDED]

    findings, seen = check(paths)

    if seen == 0:
        print("check_ccsds_isolation: read 0 files — a check that looked at")
        print("  nothing has not passed, it has not run.")
        return 1

    if not findings:
        print(
            f"check_ccsds_isolation: OK — {seen} file(s), the general frame "
            "primitive is free of ccsds_tm"
        )
        return 0

    print("check_ccsds_isolation: FAIL")
    print()
    for f in findings:
        print(f"  {f}")
    print()
    print("  `wfm/wfm_frame.h` states this about itself: `ccsds_tm` must")
    print("  depend on it to describe a CADU, so it must not call")
    print("  `ccsds_tm`'s kernels, or the two form a cycle.")
    print()
    print("  A standard's kernels reach the assembler through the ops table")
    print("  a caller passes in — that is what `wfm_frame_ops_t` is for.")
    print("  See docs/design/frame-description.md.")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
