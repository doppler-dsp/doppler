#!/usr/bin/env python3
"""Gate: a checked size product, and a double-to-size_t range check, each
have ONE home: `dp_size_mul()` and `dp_size_from_double()`.

A size product that wraps allocates small and is then written past. The
guard against it is one line -- `n > SIZE_MAX / k` -- and that is why it
went wrong: written by hand at every site, it was spelled a different way
each time (`SIZE_MAX / a / b`, a strict `<` against the quotient, a guard
on one factor of a three-way product), and correlator_core.c alone grew
three wrapping products in two review rounds. `dp_size_mul()` in
`native/inc/doppler/clib_common.h` is its one home, with a saturating face
`dp_size_mul_sat()` for an upper bound; this gate keeps it there.

Its sibling is the "does this double fit a size_t" check, spelled
`d <= (double)SIZE_MAX`. Where size_t is 64 bits that bound is wrong:
SIZE_MAX rounds UP to 2^64 as a double, so 2^64 itself passes and its
conversion is undefined. `dp_size_from_double()` bounds against 2^w
exactly; a `(double)SIZE_MAX` cast anywhere else is a site too.

**A site is one of three shapes**, in C under `native/` and `ffi/`, with
SIZE_MAX spelled any of the ways C spells it (`SIZE_MAX`, `~(size_t)0`,
`(size_t)-1`):

1. *SIZE_MAX in the numerator of a division* -- `SIZE_MAX / k`,
   `(SIZE_MAX - x) / k`, `(SIZE_MAX) / k`: the hand-written multiply (or
   multiply-add) guard. Use dp_size_mul, and dp_size_add for the sum.
2. *SIZE_MAX cast to a floating type* -- `(double)SIZE_MAX`,
   `(float)(SIZE_MAX ...)`, `(long double)SIZE_MAX`: the double-fits check.
   Use dp_size_from_double.
3. *SIZE_MAX compared in a clause that also holds a floating literal or
   a floating cast* -- `d * 1.0 <= SIZE_MAX`, `(double)n > SIZE_MAX`: the
   same check, spelled with an implicit conversion. A clause is the text
   between `&&`, `||`, `,`, `;`, `?` and `:`, so `x != 0.0f || n !=
   SIZE_MAX` is two clauses and not a site. **The limit:** the script
   cannot type-check, so `d <= SIZE_MAX` where `d` is a double and
   nothing in its clause says so is not caught. Review is the gate for
   that one.

A site is one OCCURRENCE of SIZE_MAX in a guard shape: one spelling read
by two shapes -- `(double)(SIZE_MAX / 2)` -- is one site, and a second
guard appended to a ratcheted line is a second site, so it raises the
count. Comments are blanked first
(_c_source.strip_comments), so a comment that quotes the idiom is not one.
Not scanned: clib_common.h (the home), `native/tests/**` (tests build
boundary values on purpose), `vendor/**` (not ours), and any file whose
first line is `/* jm:generated` (owned by the just-makeit generator; a
fix there is a jm fix). A `SIZE_MAX` that is neither divided, cast to a
floating type nor compared beside one -- a u64-to-size_t range check, a
saturating sum -- is not a site.

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
#: SIZE_MAX as C spells it: the macro, all-ones by complement, or -1 cast.
#: SIZE_MAX as C spells it: the macro; all-ones by complement, `~(size_t)0`,
#: `~((size_t)0)` or `(size_t)~0`; and -1 cast, `(size_t)-1`, `(size_t)(-1)`,
#: with any integer suffix. `sizeof (size_t) - 1` is a width, not a cast of
#: -1: the look-behinds keep it out (`sizeof(` and `sizeof (`, the two
#: spellings clang-format leaves).
_SFX = r"[uUlL]*"
MAXV = (
    r"(?:\bSIZE_MAX\b"
    r"|~\s*\(\s*size_t\s*\)\s*0" + _SFX + r"\b"
    r"|~\s*\(\s*\(\s*size_t\s*\)\s*0" + _SFX + r"\s*\)"
    r"|\(\s*size_t\s*\)\s*~\s*0" + _SFX + r"\b"
    r"|(?<!sizeof)(?<!sizeof )\(\s*size_t\s*\)\s*"
    r"(?:-\s*1" + _SFX + r"\b|\(\s*-\s*1" + _SFX + r"\s*\)))"
)
_MAXV = re.compile(MAXV)
#: Shape 1, the direct spelling: SIZE_MAX right before a `/`.
DIV_DIRECT = re.compile(MAXV + r"\s*/(?![/*=])")
#: Shape 2: SIZE_MAX cast to a floating type, through any parentheses.
FLOAT_CAST = re.compile(
    r"\(\s*(?:long\s+)?(?:double|float)\s*\)\s*\(*\s*" + MAXV
)
#: Shape 3: SIZE_MAX on either side of a comparison ...
CMP = re.compile(
    r"(?:[<>]=?|[!=]=)\s*\(*\s*"
    + MAXV
    + r"|"
    + MAXV
    + r"\s*\)*\s*(?:[<>]=?|[!=]=)"
)
#: ... in a clause that says it is floating: a floating literal or cast.
FLOATY = re.compile(
    r"(?<![\w.])(?:\d+\.\d*|\.\d+|\d+[eE][+-]?\d+|0[xX][\da-fA-F.]*[pP][+-]?\d+)"
    r"(?:[eE][+-]?\d+)?[fFlL]?(?![\w.])"
    r"|\(\s*(?:long\s+)?(?:double|float)\s*\)"
)


def _paren_before(text: str, close: int) -> int | None:
    """Where the `(` matching the `)` at ``close`` is."""
    depth = 0
    for i in range(close, -1, -1):
        if text[i] == ")":
            depth += 1
        elif text[i] == "(":
            depth -= 1
            if depth == 0:
                return i
    return None


def _enclosing(text: str, at: int) -> tuple[int, int]:
    """The span inside the innermost parentheses around offset ``at``, or
    the whole text when there are none. The `)` that closes an `if`'s
    controlling expression ends the comparison's context there, so the
    `1.0` of a brace-less `if (n == SIZE_MAX) return 1.0;` is not in it."""
    depth = 0
    lo = -1
    for i in range(at - 1, -1, -1):
        c = text[i]
        if c == ")":
            depth += 1
        elif c == "(":
            if depth == 0:
                lo = i
                break
            depth -= 1
    if lo < 0:
        return 0, len(text)
    depth = 0
    for j in range(at, len(text)):
        c = text[j]
        if c == "(":
            depth += 1
        elif c == ")":
            if depth == 0:
                return lo + 1, j
            depth -= 1
    return lo + 1, len(text)


#: What ends a clause around a comparison.
_CLAUSE_END = re.compile(r"&&|\|\||[,;?:{}]")


def _clause(text: str, at: int) -> str:
    """The clause of ``text`` holding offset ``at``: bounded by the
    innermost parentheses around it, and within them by `&&`, `||`, `,`,
    `;`, `?`, `:` and braces."""
    lo, hi = _enclosing(text, at)
    for m in _CLAUSE_END.finditer(text, lo, at):
        lo = m.end()
    m = _CLAUSE_END.search(text, at, hi)
    return text[lo : m.start() if m else hi]


def _maxv_in(text: str, lo: int, hi: int) -> set[int]:
    """Where each SIZE_MAX spelling in ``text[lo:hi]`` starts."""
    return {m.start() for m in _MAXV.finditer(text, lo, hi)}


def _divisions(text: str) -> set[int]:
    """Each SIZE_MAX occurrence in the numerator of a division, by offset."""
    out: set[int] = set()
    for m in DIV_DIRECT.finditer(text):
        out |= _maxv_in(text, m.start(), m.end())
    for m in re.finditer(r"\)\s*/(?![/*=])", text):
        open_at = _paren_before(text, m.start())
        if open_at is not None:
            out |= _maxv_in(text, open_at, m.start() + 1)
    return out


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
    """``{path: [(line, text)]}`` of every hand-written guard; ``text`` ends
    with the primitive that replaces it."""
    found: dict[str, list[tuple[int, str]]] = {}
    for rel in _sources(root):
        raw = (root / rel).read_text(encoding="utf-8", errors="replace")
        if raw.lstrip().startswith(GENERATED):
            continue
        text = strip_comments(raw)
        lines = text.splitlines()
        # A site is one SIZE_MAX OCCURRENCE in a guard shape, keyed by its
        # offset: one spelling read by two shapes -- `(double)(SIZE_MAX /
        # 2)` -- is one site, and a second guard appended to a ratcheted
        # line is a second one.
        floating: set[int] = set()
        for m in FLOAT_CAST.finditer(text):
            floating |= _maxv_in(text, m.start(), m.end())
        for m in CMP.finditer(text):
            if FLOATY.search(_clause(text, m.start())):
                floating |= _maxv_in(text, m.start(), m.end())
        for off in sorted(floating | _divisions(text)):
            at = text.count("\n", 0, off)
            home = (
                "dp_size_from_double"
                if off in floating
                else "dp_size_mul (dp_size_add for a sum)"
            )
            found.setdefault(rel, []).append(
                (at + 1, f"{lines[at].strip()}  -- use {home} (clib_common.h)")
            )
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
            "  an upper bound that saturates at SIZE_MAX; and\n"
            "  dp_size_from_double for a double that must fit a size_t\n"
            "  ((double)SIZE_MAX rounds up to 2^64 on LP64)."
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
