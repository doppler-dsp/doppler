#!/usr/bin/env python3
"""Gate: a throwaway build leaves src/'s compiled artifacts as it found them.

`make warnings-check` configures two throwaway trees (gcc, then clang) with
`BUILD_PYTHON=ON`, and until doppler#2518 their Python extensions and the
`wfmgen` binary the package ships landed where every build's do: in
`src/doppler/`, through the default `PYTHON_PACKAGE_DIR`. The editable install
imports from there, so after the gate ran, every Python test, example and
validator in that checkout measured the CLANG build while the C side stayed
gcc. Nothing failed where the swap happened; it failed downstream, as a STALE
certification report, two "twins" that disagreed and a wfmgen byte-parity
miss, each of which read like a real defect and cost an hour to bisect.

The fix points the throwaway trees' `PYTHON_PACKAGE_DIR` into the throwaway
tree. This is the gate that keeps it fixed:

    check_src_untouched.py snapshot FILE   # before the builds
    check_src_untouched.py compare FILE    # after: fail on any difference

It records every COMPILED artifact under `src/` -- any file that starts with
an ELF, Mach-O, PE or `ar` magic, so a `.so`, a `.pyd`, an executable under
`_bin/` or a static archive, whatever its name -- with its SHA-256, and the
compare fails on one added, changed or removed. Compiled artifacts only, and
on purpose: the builds take minutes, and a source file edited meanwhile is
the developer's, not the build's. A fresh checkout (CI) has none, so there
the gate is "the build added none", which is exactly the regression.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

#: The leading bytes of a compiled artifact: ELF; Mach-O, both byte orders,
#: 32 and 64 bit, and the universal (fat) header; a PE executable or .pyd;
#: an `ar` archive.
MAGICS = (
    b"\x7fELF",
    b"\xfe\xed\xfa\xce",
    b"\xfe\xed\xfa\xcf",
    b"\xce\xfa\xed\xfe",
    b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe",
    b"MZ",
    b"!<arch>\n",
)


def artifacts(root: Path) -> dict[str, str]:
    """Every compiled artifact under @p root: its relative path -> SHA-256."""
    out: dict[str, str] = {}
    for p in sorted(root.rglob("*")):
        if p.is_symlink() or not p.is_file():
            continue
        with p.open("rb") as f:
            head = f.read(8)
        if not any(head.startswith(m) for m in MAGICS):
            continue
        out[p.relative_to(root).as_posix()] = hashlib.sha256(
            p.read_bytes()
        ).hexdigest()
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("mode", choices=("snapshot", "compare"))
    ap.add_argument("file", type=Path, help="the snapshot (JSON)")
    ap.add_argument(
        "--root",
        type=Path,
        default=REPO / "src",
        help="the tree to watch (default: the repo's src/)",
    )
    a = ap.parse_args(argv)
    now = artifacts(a.root)
    if a.mode == "snapshot":
        a.file.write_text(json.dumps(now, indent=1) + "\n", encoding="utf-8")
        return 0

    before = json.loads(a.file.read_text(encoding="utf-8"))
    added = sorted(set(now) - set(before))
    removed = sorted(set(before) - set(now))
    changed = sorted(k for k in set(now) & set(before) if now[k] != before[k])
    if not (added or removed or changed):
        print(
            f"check_src_untouched: OK -- {len(now)} compiled artifact(s) "
            f"under {a.root.name}/, none added, changed or removed"
        )
        return 0
    print(
        f"check_src_untouched: FAIL -- the build wrote compiled artifacts "
        f"into {a.root.name}/, where the editable install imports them:"
    )
    for tag, rows in (
        ("added", added),
        ("changed", changed),
        ("removed", removed),
    ):
        for r in rows:
            print(f"  {tag:8s} {r}")
    print(
        "\n  Every later gate in this checkout would now run against that\n"
        "  build (doppler#2518). Point the throwaway tree's\n"
        "  PYTHON_PACKAGE_DIR inside it, as warnings-check and coverage do,\n"
        "  then `make build pyext` to put this tree's own artifacts back."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
