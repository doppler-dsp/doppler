#!/usr/bin/env python3
"""A header's example must call the functions in that header correctly.

``docs/**`` fenced C blocks are compiled ``-Werror`` against
``libdoppler.a`` by ``test_c_doc_snippets.py``. A **header's** ``@code``
block is compiled by nothing — it is rendered by doxygen, published to
``docs/c-api/**``, and, for a constructor, transplanted by jm into the
``.pyi`` a Python user reads. None of those paths type-check it.

So ``mpsk_receiver_create()``'s example passed **sixteen** arguments to a
**fifteen**-parameter function, and had done for as long as the parameter it
still carried had been retired (doppler#1082). Fifteen unlabelled positional
literals are why: nobody counts commas, and nothing else was going to.

This is the cheap half of that issue — **arity only**, no compiler. For every
call in an example block to a function *declared in the same header*, the
argument count must match the declaration. That needs no translation unit, no
stand-in preamble for the undeclared locals an example fragment references,
and no decision about which blocks are deliberately illustrative. It catches
the whole class of "the signature changed and the example did not", which is
the one that actually happened.

What it deliberately does NOT do: type-check, resolve macros, or look at
calls to functions declared elsewhere. Those want the compiler, and that is
the other half of doppler#1082.

Usage
-----
    python scripts/check_header_example_arity.py    # exit 1 on any mismatch
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INC = ROOT / "native" / "inc"

#: A C keyword that can head a declaration; never a function being declared.
#: Without this, `if (a, b)` and friends read as calls to `if`.
_NOT_A_NAME = {
    "if",
    "for",
    "while",
    "switch",
    "return",
    "sizeof",
    "defined",
    "do",
    "else",
}

#: `name (params)` terminated by `;` (a declaration) or `{` (a header-inline
#: definition). `[^;{()]*` on the head keeps the match anchored to one
#: declarator rather than sprawling across statements.
_DECL = re.compile(
    r"(?<![\w.>-])(\w+)\s*\(([^;{]*?)\)\s*(?:;|\{)",
    re.DOTALL,
)

_CODE_BLOCK = re.compile(r"@code\b(.*?)@endcode", re.DOTALL)


def _split_top_level(text: str) -> list[str]:
    """Split on commas that are not inside parens or brackets.

    A function-pointer parameter (`int (*cb)(void *, size_t)`) and a call
    with a nested call (`f (g (a, b), c)`) both put commas below the top
    level, and counting those is the whole way to get this wrong.
    """
    out, depth, cur = [], 0, ""
    for ch in text:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return out


def _count(params: str) -> int | None:
    """Parameters in a declaration, or ``None`` when it must not be checked.

    ``None`` for a variadic function: an example may legitimately pass any
    number of trailing arguments, so a count says nothing.
    """
    p = params.strip()
    if not p or p == "void":
        return 0
    parts = [x.strip() for x in _split_top_level(p)]
    if any(x == "..." for x in parts):
        return None
    return len(parts)


def _strip_comment_markers(block: str) -> str:
    """Drop the ` * ` that opens each line inside a doxygen comment."""
    return "\n".join(re.sub(r"^\s*\*\s?", "", ln) for ln in block.splitlines())


def _strip_block_comments(src: str) -> str:
    """Blank out comments, preserving newlines so line numbers survive."""

    def blank(m: re.Match[str]) -> str:
        return re.sub(r"[^\n]", " ", m.group(0))

    src = re.sub(r"/\*.*?\*/", blank, src, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", blank, src)


def declarations(src: str) -> dict[str, int | None]:
    """Function name -> parameter count, for one header's own declarations."""
    out: dict[str, int | None] = {}
    for m in _DECL.finditer(_strip_block_comments(src)):
        name = m.group(1)
        if name in _NOT_A_NAME:
            continue
        n = _count(m.group(2))
        # A name declared twice with different arities cannot be judged;
        # drop it rather than guess which one an example meant.
        if name in out and out[name] != n:
            out[name] = None
        else:
            out[name] = n
    return out


def calls(code: str, known: set[str]) -> list[tuple[str, int]]:
    """Every call to a *known* name in an example block, with its arity."""
    found: list[tuple[str, int]] = []
    for m in re.finditer(r"(?<![\w.>-])(\w+)\s*\(", code):
        name = m.group(1)
        if name not in known:
            continue
        i = m.end() - 1
        depth, j = 0, i
        while j < len(code):
            if code[j] in "([{":
                depth += 1
            elif code[j] in ")]}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        else:
            continue  # unbalanced; not something to judge
        args = code[i + 1 : j].strip()
        n = 0 if not args else len(_split_top_level(args))
        found.append((name, n))
    return found


def check_header(path: Path, root: Path) -> list[tuple[str, str, str]]:
    """Disagreements in one header, as ``(relpath, function, message)``."""
    src = path.read_text(encoding="utf-8")
    decls = declarations(src)
    checkable = {k for k, v in decls.items() if v is not None}
    if not checkable:
        return []
    rel = path.relative_to(root)
    problems: list[tuple[str, str, str]] = []
    for block in _CODE_BLOCK.finditer(src):
        line = src[: block.start()].count("\n") + 1
        code = _strip_comment_markers(block.group(1))
        # MOST example blocks in this tree are PYTHON doctests -- jm renders
        # them into the `.pyi` a user reads, which is exactly why they are
        # written here. A Python call has the binding's arity, not the C
        # function's (`add_q15(a, b)` against a 5-parameter kernel), so
        # judging one against the C declaration is pure noise. The `>>>` is
        # the discriminator, and it is the same one jm keys on.
        if ">>>" in code:
            continue
        for name, got in calls(code, checkable):
            want = decls[name]
            if got != want:
                problems.append(
                    (
                        str(rel),
                        name,
                        f"{rel}:{line}: example calls {name}() with {got} "
                        f"argument(s); it is declared with {want}",
                    )
                )
    return problems


#: Existing breakage, RATCHETED: it may only shrink. Keyed on
#: `<header> <function>` and never on a line number, because a line number
#: moves whenever anything above it is edited and the waiver then covers the
#: wrong thing silently.
RATCHET = INC / ".example-arity-ratchet"


def _ratchet(path: Path) -> set[tuple[str, str]]:
    if not path.exists():
        return set()
    out = set()
    for ln in path.read_text(encoding="utf-8").splitlines():
        ln = ln.split("#", 1)[0].strip()
        if not ln:
            continue
        hdr, _, fn = ln.rpartition(" ")
        out.add((hdr.strip(), fn.strip()))
    return out


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    # A gate that can only run against the real tree cannot be sabotaged --
    # you would have to break doppler to test it, and nobody does that twice.
    ap.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="repo root to scan (default: this checkout)",
    )
    args = ap.parse_args(argv[1:])
    root = args.root.resolve()
    inc = root / "native" / "inc"
    ratchet_path = inc / ".example-arity-ratchet"

    headers = sorted(inc.rglob("*.h"))
    found: list[tuple[str, str, str]] = []
    for h in headers:
        for key_path, key_fn, msg in check_header(h, root):
            found.append((key_path, key_fn, msg))

    allowed = _ratchet(ratchet_path)
    seen = {(p, f) for p, f, _ in found}
    new = [m for p, f, m in found if (p, f) not in allowed]
    # The other direction, which is the one that shipped broken elsewhere in
    # this repo: a waiver kept after its example was fixed. Without this, the
    # next regression in that example passes.
    stale = sorted(allowed - seen)

    if new or stale:
        print(
            "check_header_example_arity: an example disagrees with the "
            "signature above it — FAIL"
        )
        for m in new:
            print(f"  {m}")
        for p, f in stale:
            print(
                f"  {p}: {f}() is in {RATCHET.name} but no longer disagrees "
                "— delete the line, the ratchet may only shrink"
            )
        if new:
            print(
                "\n  A header's example block is compiled by nothing, so it\n"
                "  can drift from its own declaration silently. Name the\n"
                "  arguments (see mpsk_receiver_core.h) — fifteen positional\n"
                "  literals is how the last one went unnoticed. doppler#1082."
            )
        return 1
    print(
        f"check_header_example_arity: OK — {len(headers)} header(s), every "
        f"example call matches its declaration ({len(allowed)} ratcheted, "
        "may only shrink)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
