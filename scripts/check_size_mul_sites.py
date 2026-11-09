#!/usr/bin/env python3
"""Gate: a checked size product has ONE home, `dp_size_mul()`.

A size product that wraps allocates small and is then written past. The
guard against it is one line -- `n > SIZE_MAX / k` -- and that is why it
went wrong: written by hand at every site, it was spelled a different way
each time (`SIZE_MAX / a / b`, a strict `<` against the quotient, a guard
on one factor of a three-way product), and correlator_core.c alone grew
three wrapping products in two review rounds. `dp_size_mul()` in
`native/inc/doppler/clib_common.h` is its one home, with a saturating face
`dp_size_mul_sat()` for an upper bound; this gate keeps it there.

**A site is `SIZE_MAX` used as a divisor's dividend** (`SIZE_MAX\\s*/`),
the signature of a hand-written multiply guard, in C under `native/` and
`ffi/`. Comments are blanked first (_c_source.strip_comments), so a
comment that quotes the idiom is not one. Not scanned: clib_common.h (the
home), `native/tests/**` (tests build boundary values on purpose),
`vendor/**` (not ours), and any file whose first line is
`/* jm:generated` (owned by the just-makeit generator; a fix there is a jm
fix). A `SIZE_MAX` that is not divided -- a u64-to-size_t range check, a
saturating SUM -- is not a product and is not a site.

**What is left is a RATCHET that only shrinks.** `scripts/.size-mul-ratchet`
holds lines `<path> <count>  # reason`. A file may hold no more sites than
its line says, a file not listed may hold none, a count left above what
the file now has is stale and fails, and nothing may be added or raised
against the merge base with `--base` (_gitbase.added_since_base, the one
ratchet read, as check_ring_internals.py uses it).

Usage:  python3 scripts/check_size_mul_sites.py [--root DIR] [--base REF]
            [--list]
Exit 0 when every site is within the ratchet.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from _c_source import strip_comments
from _gitbase import BaseUnreadableError, added_since_base, in_git_repo

ROOT = Path(__file__).resolve().parent.parent
OWNER = "native/inc/doppler/clib_common.h"
BASELINE = "scripts/.size-mul-ratchet"
SCAN_DIRS = ("native", "ffi")
EXCLUDE_DIRS = ("native/tests/",)
GENERATED = "/* jm:generated"
SITE = re.compile(r"\bSIZE_MAX\s*/")


def _sources(root: Path) -> list[str]:
    out: list[str] = []
    for d in SCAN_DIRS:
        for path in sorted((root / d).rglob("*")):
            if path.suffix not in (".c", ".h") or not path.is_file():
                continue
            rel = path.relative_to(root).as_posix()
            if rel == OWNER or rel.startswith(EXCLUDE_DIRS):
                continue
            if "vendor" in rel.split("/"):
                continue
            out.append(rel)
    return out


def sites(root: Path) -> dict[str, list[tuple[int, str]]]:
    """``{path: [(line, text)]}`` of every hand-written multiply guard."""
    found: dict[str, list[tuple[int, str]]] = {}
    for rel in _sources(root):
        raw = (root / rel).read_text(encoding="utf-8", errors="replace")
        if raw.lstrip().startswith(GENERATED):
            continue
        text = strip_comments(raw)
        lines = text.splitlines()
        for m in SITE.finditer(text):
            at = text.count("\n", 0, m.start())
            found.setdefault(rel, []).append((at + 1, lines[at].strip()))
    return found


def _baseline_text(root: Path) -> str:
    path = root / BASELINE
    return path.read_text(encoding="utf-8") if path.exists() else ""


def parse(text: str) -> dict[str, tuple[int, str]]:
    """``<path> <count>  # reason`` per line -> ``{path: (count, reason)}``."""
    out: dict[str, tuple[int, str]] = {}
    for raw in text.splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        body, _, reason = raw.partition("#")
        rel, _, n = body.strip().rpartition(" ")
        out[rel.strip()] = (int(n), reason.strip())
    return out


def _entries(text: str) -> list[str]:
    """A count N as the entries ``path 0`` .. ``path N``: what makes a raise
    an ADDED entry for added_since_base, and a shrink not one."""
    return [
        f"{rel} {k}"
        for rel, (n, _) in parse(text).items()
        for k in range(n + 1)
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument(
        "--base",
        default="origin/main",
        help="ref whose merge base holds the ratchet this one may only "
        "shrink from",
    )
    ap.add_argument(
        "--list", action="store_true", help="print every site, then exit 0"
    )
    a = ap.parse_args()
    root = a.root.resolve()
    found = sites(root)
    if a.list:
        for rel, at in sorted(found.items()):
            print(f"{rel} {len(at)}")
            for n, line in at:
                print(f"    {rel}:{n}: {line}")
        return 0

    text = _baseline_text(root)
    allowed = parse(text)
    bad: list[str] = []
    for rel, (_n, reason) in sorted(allowed.items()):
        if not reason:
            bad.append(f"{BASELINE}: '{rel}' has no reason on its line")
    for rel, at in sorted(found.items()):
        cap = allowed.get(rel, (0, ""))[0]
        if len(at) > cap:
            bad.append(
                f"{rel}: {len(at)} hand-written SIZE_MAX guard(s), "
                f"the ratchet allows {cap}"
            )
            bad += [
                f"    {rel}:{n}: {line}  -- use dp_size_mul (clib_common.h)"
                for n, line in at
            ]
    for rel, (n, _) in sorted(allowed.items()):
        now = len(found.get(rel, []))
        if now < n:
            bad.append(
                f"{BASELINE}: '{rel}' allows {n} but the file has {now} -- "
                "bring the count down; it only shrinks"
            )
    if in_git_repo(root):
        try:
            added = added_since_base(
                root,
                a.base,
                BASELINE,
                _entries(text),
                _entries,
                since="scripts/check_size_mul_sites.py",
            )
        except BaseUnreadableError:
            bad.append(
                f"{BASELINE}: cannot resolve {a.base}, so a raised count "
                "cannot be told from an old one. Fetch it: git fetch "
                "--no-tags --depth=1 origin "
                "+refs/heads/main:refs/remotes/origin/main"
            )
            added = []
        grown: dict[str, int] = {}
        for entry in added:
            rel, _, k = entry.rpartition(" ")
            grown[rel] = max(grown.get(rel, 0), int(k))
        bad += [
            f"{BASELINE}: '{rel}' rose to {k} since the merge base with "
            f"{a.base} -- the list only shrinks; use dp_size_mul "
            "(clib_common.h)"
            for rel, k in sorted(grown.items())
        ]
    if bad:
        print("size-mul: a size product guarded by hand --")
        for b in bad:
            print(f"  {b}")
        print(
            "\n  Use dp_size_mul (clib_common.h): it returns 0 and leaves\n"
            "  *out untouched when a * b would wrap, or dp_size_mul_sat for\n"
            "  an upper bound that saturates at SIZE_MAX."
        )
        return 1
    total = sum(len(v) for v in found.values())
    print(
        f"size-mul: OK -- {total} ratcheted site(s) in {len(found)} "
        "file(s); no new one"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
