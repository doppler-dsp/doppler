#!/usr/bin/env python3
"""Gate: a ring's head, tail and mask belong to buffer.h alone (doppler#1426).

`dp_<t>_t` (native/inc/doppler/buffer/buffer.h) is a lock-free SPSC ring,
and every consumer used to rebuild its operations by hand from the struct:
free space as `capacity - (head - tail)`, a frame pointer as
`data + (tail & mask) * 2`, a reset as two stores. acq, detector and
detector2d each carried about ten of those until #1895 moved them onto the
ring's own API: write_some, peek, consume and reset, and the framer. This
gate holds them there. Outside buffer.h, no code reads or writes a ring's
`head`, `tail` or `mask`.

**A ring is known by its declaration, not by a field name.** `head` and
`mask` are common fields elsewhere: an LFSR's mask, a Viterbi traceback's
head, a delay line's own index. The ring types are read from the macro's
own instantiations, `DECLARE_DP_BUFFER (name, ...)` -> `dp_<name>_t`:
buffer.h's f32, f64 and i16, and any a consumer declares for itself
(dp_tlm's `tlmr`). A ring is an identifier declared as a pointer to one
in the file being read, or a struct member declared that way in any
header. `X->head`, `X->tail` or `X->mask` on one is a site.

Scope: library C, its examples, its validation harnesses and its
benchmarks (native/inc, native/src, native/examples, native/validation,
native/benchmarks), less buffer.h itself. Tests are not scanned: the
buffer's own tests are oracles of its internals.

**Read as the compiler reads it.** Comments are blanked first, with
_c_source.strip_comments, so documentation that quotes a forbidden access
is not one (#1984's class), and an access is matched over the whole file,
not line by line: clang-format breaks a long chain before its `->`, and
`s->hist` on one line with `->head` on the next is one access.

**The baseline is a RATCHET that only shrinks.** It holds lines of the
form `<path> <count>  # reason`. A file may touch a ring's internals no
more often than its line says, and a file not listed may not touch them
at all. A count left above what the file now does is stale and fails, so
it must come down. Nothing may be added or raised against the merge base
with `--base`. That is _gitbase.added_since_base, the one ratchet read: a
count N reads as the entries `path 0` .. `path N`, so a raise or a new
file is an ADDED entry and a shrink is not.

Usage:  python3 scripts/check_ring_internals.py [--root DIR] [--base REF]
            [--list]
Exit 0 when every site is within the baseline.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from _c_source import strip_comments
from _gitbase import BaseUnreadableError, added_since_base, in_git_repo

ROOT = Path(__file__).resolve().parent.parent
OWNER = "native/inc/doppler/buffer/buffer.h"
BASELINE = "scripts/.ring-internals-ratchet"
SCAN_DIRS = (
    "native/inc",
    "native/src",
    "native/examples",
    "native/validation",
    "native/benchmarks",
)

#: A ring type: one instantiation of buffer.h's macro.
_INSTANCE = re.compile(r"^\s*DECLARE_DP_BUFFER\s*\(\s*(\w+)\s*,", re.M)


def _decl_patterns(types: list[str]) -> tuple[re.Pattern, re.Pattern]:
    """An identifier declared as a pointer to a ring (a local, a parameter
    or a member), and the same as a struct MEMBER only (a declaration that
    ends in `;`): a header's members name rings in every file, its
    parameters (a `state` in a prototype) only in their own. A cast has no
    identifier after its `*`; a function returning a ring names a function,
    which is never dereferenced."""
    t = r"\bdp_(?:" + "|".join(map(re.escape, types)) + r")_t\s*\*+\s*"
    ident = r"(?:const\s+)?([A-Za-z_]\w*)"
    return re.compile(t + ident), re.compile(t + ident + r"\s*;")


def _sources(root: Path) -> list[str]:
    out: list[str] = []
    for d in SCAN_DIRS:
        for path in sorted((root / d).rglob("*")):
            if path.suffix in (".c", ".h") and path.is_file():
                rel = path.relative_to(root).as_posix()
                if rel != OWNER:
                    out.append(rel)
    return out


def sites(root: Path) -> dict[str, list[tuple[int, str]]]:
    """``{path: [(line, text)]}`` of every ring-internal access."""
    files = _sources(root)
    texts = {
        rel: strip_comments(
            (root / rel).read_text(encoding="utf-8", errors="replace")
        )
        for rel in files
    }
    owner = (root / OWNER).read_text(encoding="utf-8", errors="replace")
    types = sorted(
        set(_INSTANCE.findall(owner))
        | {t for text in texts.values() for t in _INSTANCE.findall(text)}
    )
    decl, member = _decl_patterns(types)
    members = {
        name
        for rel, text in texts.items()
        if rel.endswith(".h")
        for name in member.findall(text)
    }
    found: dict[str, list[tuple[int, str]]] = {}
    for rel, text in texts.items():
        names = members | set(decl.findall(text))
        if not names:
            continue
        access = re.compile(
            r"\b(?:" + "|".join(sorted(map(re.escape, names))) + r")"
            r"\s*->\s*(?:head|tail|mask)\b"
        )
        # The whole text, so `->` may follow a line break; a site is
        # reported at the line its `->` is on.
        lines = text.splitlines()
        for m in access.finditer(text):
            at = text.count("\n", 0, m.start(0) + m.group(0).index("->"))
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
        help="ref whose merge base holds the baseline this one may only "
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
                f"{rel}: {len(at)} access(es) to a ring's head/tail/mask, "
                f"the baseline allows {cap}"
            )
            bad += [f"    {rel}:{n}: {line}" for n, line in at]
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
                since="scripts/check_ring_internals.py",
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
            f"{a.base} -- the list only shrinks; use the ring's API "
            "(#1426)"
            for rel, k in sorted(grown.items())
        ]
    if bad:
        print("ring-internals: a ring's head/tail/mask outside buffer.h --")
        for b in bad:
            print(f"  {b}")
        print(
            "\n  Use the ring's own operations (buffer.h): dp_<t>_space,\n"
            "  dp_<t>_write_some, dp_<t>_peek, dp_<t>_consume, dp_<t>_reset,\n"
            "  or the framer (dp_<t>_framer_feed / _next) for fixed frames."
        )
        return 1
    total = sum(len(v) for v in found.values())
    print(
        f"ring-internals: OK -- {total} baselined site(s) in "
        f"{len(found)} file(s); no new one"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
