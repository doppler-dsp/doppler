#!/usr/bin/env python3
"""The allocation gate's second rule: no size written as an inline product.

`scripts/check_alloc_helpers.py` (make lint-alloc-helpers) runs it; its
docstring states the rule. It lives in its own module so that its ratchet,
`scripts/.alloc-product-ratchet`, has a rule file of its own to name as
`since` (scripts/_gitbase.py): the first rule's script was on main before
this one existed, so it cannot tell "no baseline yet" from "the list was
renamed".
"""

from __future__ import annotations

import re
from typing import TYPE_CHECKING

from _c_source import strip_comments
from _gitbase import BaseUnreadableError, added_since_base, in_git_repo

if TYPE_CHECKING:
    from pathlib import Path

#: This rule's own file, the `since` of its ratchet.
RULE = "scripts/_alloc_products.py"

# --------------------------------------------------------------------------- #
# The second rule: a size written as an inline product                        #
# --------------------------------------------------------------------------- #

#: Every allocator and the arguments that carry a size. A product in ANY
#: of them can wrap before the allocator sees it: calloc checks nmemb *
#: size, but `calloc (a * b, size)` has already wrapped `a * b` by then,
#: and so has `dp_xmallocn (a * b, size)`. The look-behind keeps
#: `dp_xmalloc` from matching `malloc` and a member such as
#: `p->realloc_hook` from matching at all.
SIZE_ARGS = {
    "malloc": (0,),
    "dp_xmalloc": (0,),
    "realloc": (1,),
    "dp_xrealloc": (1,),
    "calloc": (0, 1),
    "dp_xcalloc": (0, 1),
    "dp_mallocn": (0, 1),
    "dp_xmallocn": (0, 1),
    "dp_xreallocn": (1, 2),
}
SIZED = re.compile(
    r"(?<![\w.>])("
    + "|".join(sorted(SIZE_ARGS, key=len, reverse=True))
    + r")\s*\("
)
#: A `*` right after one of these is a dereference, not a product.
_UNARY_AFTER = {"sizeof", "return", "case"}
#: The jm generator's marker, on a file's first line.
JM_GENERATED = "/* jm:generated"


def _call_args(text: str, at: int) -> list[str] | None:
    """The comma-separated arguments of the call whose `(` ends at ``at``,
    split at depth 0 only; None when the parentheses never close."""
    depth, start, out = 0, at, []
    for j in range(at, len(text)):
        c = text[j]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                out.append(text[start:j])
                return out
            depth -= 1
        elif c == "," and depth == 0:
            out.append(text[start:j])
            start = j + 1
    return None


def top_level_product(arg: str) -> bool:
    """Does ``arg`` multiply at its top level? A `*` is binary when an
    operand ends right before it (a name other than `sizeof`, a number,
    `)` or `]`); one inside parentheses is not the size's own product."""
    depth = 0
    for k, c in enumerate(arg):
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "*" and depth == 0:
            before = arg[:k].rstrip()
            if not before:
                continue
            word = re.search(r"(\w+)$", before)
            if word and word.group(1) not in _UNARY_AFTER:
                return True
            if before[-1] in ")]":
                return True
    return False


def product_sites(
    root: Path, scan_dirs: tuple[str, ...], sanctioned: str
) -> dict[str, list[tuple[int, str]]]:
    """``{path: [(line, text)]}`` of every allocation with an inline
    product in a size argument, in hand-written library C under
    ``scan_dirs``, less ``sanctioned`` (the helpers' home)."""
    found: dict[str, list[tuple[int, str]]] = {}
    for d in scan_dirs:
        base = root / d
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in (".c", ".h"):
                continue
            rel = path.relative_to(root).as_posix()
            if rel == sanctioned:
                continue
            raw = path.read_text(encoding="utf-8", errors="replace")
            if raw.lstrip().startswith(JM_GENERATED):
                continue
            text = strip_comments(raw)
            lines = text.splitlines()
            for m in SIZED.finditer(text):
                args = _call_args(text, m.end())
                if args is None:
                    continue
                if any(
                    i < len(args) and top_level_product(args[i])
                    for i in SIZE_ARGS[m.group(1)]
                ):
                    at = text.count("\n", 0, m.start())
                    found.setdefault(rel, []).append(
                        (at + 1, lines[at].strip())
                    )
    return found


PRODUCTS_HEADER = """\
# Baseline for scripts/check_alloc_helpers.py's second rule -- a RATCHET.
#
# Allocations in doppler's library C with an inline product in a size
# argument (`malloc (n * sizeof *p)`, `dp_xrealloc (p, n * size)`,
# `calloc (a * b, size)`), per file. A product that wraps asks for a SMALL
# buffer and the code then writes past it. Each one moves to dp_mallocn
# (NULL on overflow, for a path that refuses) or dp_xmallocn /
# dp_xreallocn (abort, for a trusted count), all in
# native/inc/doppler/clib_common.h, with the product passed as their two
# factors.
#
# THIS LIST MAY ONLY SHRINK, with no reason escape: every site has a
# checked spelling. A file not listed may hold none, a count left above
# what its file now has fails, and nothing may be added or raised against
# the merge base with origin/main. Re-record after converting some with
#
#     make lint-alloc-helpers-baseline
#
# Format:  <path> <count>
"""


def _parse_products(text: str) -> dict[str, int]:
    out: dict[str, int] = {}
    for raw in text.splitlines():
        body = raw.partition("#")[0].strip()
        if not body:
            continue
        rel, _, n = body.rpartition(" ")
        out[rel.strip()] = int(n)
    return out


def _product_entries(text: str) -> list[str]:
    """A count N as the entries ``path 0`` .. ``path N``: what makes a raise
    an ADDED entry for added_since_base, and a shrink not one."""
    return [
        f"{rel} {k}"
        for rel, n in _parse_products(text).items()
        for k in range(n + 1)
    ]


def write_products(path: Path, found: dict[str, list]) -> None:
    body = "".join(f"{rel} {len(at)}\n" for rel, at in sorted(found.items()))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(PRODUCTS_HEADER + "\n" + body, encoding="utf-8")


def check_products(
    root: Path,
    path: Path,
    base: str,
    scan_dirs: tuple[str, ...],
    sanctioned: str,
) -> int:
    """The second rule. 0 when every site is within the ratchet."""
    found = product_sites(root, scan_dirs, sanctioned)
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    allowed = _parse_products(text)
    rel_list = path.resolve().relative_to(root).as_posix()
    bad: list[str] = []
    for rel, at in sorted(found.items()):
        cap = allowed.get(rel, 0)
        if len(at) > cap:
            bad.append(
                f"  {rel}: {len(at)} allocation(s) sized by an inline "
                f"product, the ratchet allows {cap}"
            )
            bad += [
                f"      {rel}:{n}  {line}  -- use dp_mallocn / dp_xmallocn "
                "/ dp_xreallocn (clib_common.h)"
                for n, line in at
            ]
    for rel, cap in sorted(allowed.items()):
        now = len(found.get(rel, []))
        if now < cap:
            bad.append(
                f"  {rel_list}: '{rel}' allows {cap} but the file has "
                f"{now} -- bring the count down; it only shrinks"
            )
    if in_git_repo(root):
        try:
            # `since` is THIS module, the rule's own file, as size-mul uses
            # its script (_gitbase.added_since_base): only a base holding
            # neither the rule nor its list has no baseline yet. A list
            # renamed in the same change as a raise is still read as absent
            # at the base, and so EMPTY: every entry is added, and refused.
            added = added_since_base(
                root,
                base,
                rel_list,
                _product_entries(text),
                _product_entries,
                since=RULE,
            )
        except BaseUnreadableError:
            bad.append(
                f"  {rel_list}: cannot resolve {base}, so a raised count "
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
            f"  {rel_list}: '{rel}' rose to {k} since the merge base with "
            f"{base} -- the list only shrinks"
            for rel, k in sorted(grown.items())
        ]
    if bad:
        print(
            "FAIL: an allocation is sized by an inline product.\n"
            "\n"
            "`malloc (n * size)` wraps when n is large and returns a SMALL\n"
            "buffer. Let the allocator check the product:\n"
            "\n"
            "    p = dp_mallocn (n, sizeof *p);   /* NULL on overflow: "
            "refuse */\n"
            "    p = dp_xmallocn (n, sizeof *p);  /* trusted count: aborts "
            "*/\n"
            "    p = dp_xreallocn (p, n, sizeof *p);\n"
        )
        print("\n".join(bad))
        return 1
    total = sum(len(v) for v in found.values())
    print(
        f"alloc-product gate: OK -- {total} inline-product size(s) across "
        f"{len(found)} file(s), all ratcheted (may only shrink)"
    )
    return 0
