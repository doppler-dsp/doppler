#!/usr/bin/env python3
"""Integer-coded arguments are passed by name, never as a number.

PSD's window and averaging mode, and the Spectrogram's row units, are plain
``int`` parameters, so the compiler accepts ``0`` and ``1`` as readily as
``DP_PSD_WINDOW_HANN`` and ``DP_SPECTROGRAM_DB``, and a number says nothing
about which choice the writer meant. The rule began with the Spectrogram's
mode (the owner's rule on #1968: dB rows are asked for by name; #2097 had
renumbered the modes, and a call written as a number would have changed
meaning without a diff). The Spectrogram design review (#1894) found the
same hole three more times: 179 window and 117 averaging-mode arguments
written as numbers, one hidden behind a comment (``1 /* Kaiser */``).

**What it checks.** ``TABLE`` names each constructor whose arguments are
integer codes, the argument count of a real call, and which slots hold
which code. A call with that many arguments whose named slot is, once
comments, enclosing parentheses and casts are stripped, an integer literal
(decimal, octal, hex or binary, any ``u``/``l`` suffix, either sign) or a
character literal, is refused. A mention with another argument count
(prose's ``dp_psd_create()``) is not a call and is skipped, and a call
inside a string literal is not a call either.

**One declaration.** Each slot names an ``[[enum]]`` of just-makeit.toml,
and the names come from the C enum that carries it (``SSOT: enum=X``, held
to the manifest by ``check_wfm_enum_tables.py``'s rule 7), read with that
gate's own parser. So this file keeps no copy of any name or value.

**Registration-free.** It scans every tracked ``*.c``, ``*.h`` and ``*.md``
(``git ls-files``), so a new example, test, bench or doc page is checked the
moment it is committed. Calls are joined across lines, and a header comment's
``*`` continuation is stripped first, so a call wrapped inside an ``@code``
block is read whole. Two trees are skipped: ``docs/c-api/`` is generated from
the headers this already scans, and ``example-projects/`` builds against a
RELEASED doppler that predates the names (#2186).

**Fail-closed, per row.** Every function in ``TABLE`` must be called at least
once: a row whose function was renamed away would otherwise check nothing
and read as clean.

**What it does not refuse:** a variable initialised from a literal, or an
expression such as ``1 - 1``. A refusal test that needs an invalid value
(``-1``, 4) names it in a variable. The gate is for the slip, not the
determined.

**--fix** rewrites each refused literal to the name whose C value it is, in
place and by source position, so ``make format`` (which runs
``lint-named-args``) is the fixer the gate points at. A literal that is no
value of its enum is left alone and still refused.

Usage
-----
    python3 scripts/check_named_args.py           # exit 1 on a literal
    python3 scripts/check_named_args.py --fix     # rewrite, exit 1 if any left
    python3 scripts/check_named_args.py --root D  # scan another tree
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

from check_wfm_enum_tables import bound_enums

ROOT = Path(__file__).resolve().parent.parent

#: function -> (argument count, {argument index: (parameter, [[enum]])}).
TABLE: dict[str, tuple[int, dict[int, tuple[str, str]]]] = {
    "dp_spectrogram_create": (
        5,
        {2: ("window", "psd_window"), 4: ("mode", "spectrogram_mode")},
    ),
    "dp_psd_create": (
        9,
        {2: ("window", "psd_window"), 7: ("mode", "acc_trace_mode")},
    ),
    "dp_psd_create_hop": (
        10,
        {2: ("window", "psd_window"), 7: ("mode", "acc_trace_mode")},
    ),
    "dp_carrier_acq_create": (13, {4: ("window", "psd_window")}),
}

#: Tracked trees that are not this gate's (see the module docstring).
SKIP = ("docs/c-api/", "example-projects/")

# Longest first, so `dp_psd_create_hop` is never read as `dp_psd_create`.
_CALL = re.compile(
    r"(?<![\w.])(" + "|".join(sorted(TABLE, key=len, reverse=True)) + r")\s*\("
)

#: A line break inside a call, with a comment's continuation prefix (`*`,
#: never the `*/` that closes it, or `//`) removed, so a call wrapped in a
#: header's @code block reads as one line.
_BREAK = re.compile(r"\n[ \t]*(?:\*(?!/)|//)?")
_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
#: C string and character literals, blanked before calls are looked for so
#: a `printf ("dp_psd_create(%zu)")` is not a call. Same offsets kept.
_STRING = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])\'')

_INT = re.compile(r"[+-]?\s*(?:0[xX][0-9a-fA-F]+|0[bB][01]+|\d+)[uUlL]*")
_CHAR = re.compile(r"'(?:\\.|[^'\\])'")
_CAST = re.compile(r"\(\s*(?:const\s+)?[A-Za-z_]\w*\s*\)\s*(.+)", re.S)
#: Inside one argument: a comment to step over, or the literal to rewrite.
#: Whitespace belongs to the literal only after a sign: an unsigned literal's
#: span starts at its first digit, so the rewrite keeps the space, newline and
#: indent in front of it (the first version took them, and joined lines).
_TOKEN = re.compile(
    r"(/\*.*?\*/|//[^\n]*)"
    r"|(?<![\w.])((?:[+-]\s*)?(?:0[xX][0-9a-fA-F]+|0[bB][01]+|\d+)[uUlL]*)"
    r"(?![\w.])",
    re.S,
)


def _args(text: str, start: int) -> tuple[list[tuple[int, int]], int] | None:
    """The (start, end) span of each top-level argument of the call whose
    `(` is at `start`, with the offset just past its `)`; None when the
    parentheses never close (a fragment cut off by the file's end)."""
    depth, spans, a = 0, [], start + 1
    for i in range(start, len(text)):
        ch = text[i]
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
            if depth == 0:
                spans.append((a, i))
                return spans, i + 1
        elif ch == "," and depth == 1:
            spans.append((a, i))
            a = i + 1
    return None


def _bare(arg: str) -> str:
    """The argument with comments, enclosing parentheses and casts removed."""
    a = _BREAK.sub(" ", _COMMENT.sub(" ", arg)).strip()
    while True:
        if a.startswith("("):
            # one pair enclosing the whole argument, not `(a) + (b)`
            inner = _args(a, 0)
            if inner and inner[1] == len(a) and len(inner[0]) == 1:
                s, e = inner[0][0]
                a = a[s:e].strip()
                continue
        m = _CAST.fullmatch(a)
        if m:
            a = m.group(1).strip()
            continue
        return a


def _value(literal: str) -> int | None:
    """The integer an integer literal spells, as C reads it."""
    s = re.sub(r"[uUlL]+$", "", literal.replace(" ", ""))
    sign = -1 if s.startswith("-") else 1
    s = s.lstrip("+-")
    if re.fullmatch(r"0[0-7]+", s):
        return sign * int(s, 8)
    try:
        return sign * int(s, 0)
    except ValueError:
        return None


class Finding:
    """One refused literal, and the name that replaces it if there is one."""

    def __init__(
        self,
        line: int,
        func: str,
        param: str,
        bare: str,
        name: str | None,
        span: tuple[int, int] | None,
    ) -> None:
        self.line = line
        self.func = func
        self.param = param
        self.bare = bare
        self.name = name
        self.span = span


def check_text(
    text: str, names: dict[str, dict[int, str]], is_c: bool
) -> tuple[dict[str, int], list[Finding]]:
    """Calls checked per function, and each literal found in a named slot."""
    scan = _STRING.sub(lambda m: " " * len(m.group(0)), text) if is_c else text
    calls: dict[str, int] = {}
    found: list[Finding] = []
    for m in _CALL.finditer(scan):
        func = m.group(1)
        nargs, slots = TABLE[func]
        got = _args(scan, m.end() - 1)
        if got is None or len(got[0]) != nargs:
            continue
        calls[func] = calls.get(func, 0) + 1
        for idx, (param, enum) in slots.items():
            s, e = got[0][idx]
            bare = _bare(text[s:e])
            if not (_INT.fullmatch(bare) or _CHAR.fullmatch(bare)):
                continue
            name, span = None, None
            if _INT.fullmatch(bare):
                name = names.get(enum, {}).get(_value(bare))
                for t in _TOKEN.finditer(text, s, e):
                    if t.group(2):
                        span = t.span(2)
                        break
            line = text.count("\n", 0, m.start()) + 1
            found.append(Finding(line, func, param, bare, name, span))
    return calls, found


def tracked(root: Path) -> list[str]:
    """Every tracked C source, C header and Markdown page under `root`.

    As git names them: relative to `root`, with `/` on every OS. They are
    reported that way too, so a finding reads the same on Windows, where a
    `Path` would print `inc\\s.h` (#2097's clang-cl leg).
    """
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--", "*.c", "*.h", "*.md"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return [p for p in out.split("\0") if p and not p.startswith(SKIP)]


def _names(root: Path) -> tuple[dict[str, dict[int, str]], list[str]]:
    """value -> name for every [[enum]] a slot uses, from its C enum."""
    bound, errs = bound_enums(root)
    by_enum: dict[str, dict[int, str]] = {}
    for _, enum, pairs in bound:
        by_enum[enum] = {v: n for n, v in pairs if v is not None}
    for enum in sorted({e for _, s in TABLE.values() for _, e in s.values()}):
        if enum not in by_enum:
            errs.append(
                f"no C enum carries `SSOT: enum={enum}`, so a literal in its"
                " slots has no name to be checked or fixed against"
            )
    return by_enum, errs


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    # A gate that can only run against the real tree cannot be sabotaged
    # without breaking doppler, so its test points it at a seeded one.
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--fix", action="store_true")
    args = ap.parse_args(argv[1:])
    root = args.root.resolve()

    # Every message is ASCII: a Windows console's code page (cp1252) prints
    # an em-dash as a replacement character (#2097's clang-cl leg).
    names, errs = _names(root)
    if errs:
        for e in errs:
            print(f"check_named_args: {e} -- FAIL")
        return 1

    calls: dict[str, int] = dict.fromkeys(TABLE, 0)
    files, fixed, left = 0, 0, []
    for rel in tracked(root):
        path = root / rel
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        n, found = check_text(text, names, not rel.endswith(".md"))
        if n:
            files += 1
            for f, k in n.items():
                calls[f] += k
        todo = [f for f in found if args.fix and f.name and f.span]
        for f in found:
            if f not in todo:
                left.append((f"{rel}:{f.line}", f))
        if todo:
            # Last first, so the earlier spans' offsets still hold.
            for f in sorted(todo, key=lambda f: f.span[0], reverse=True):
                s, e = f.span
                text = text[:s] + f.name + text[e:]
            path.write_text(text, encoding="utf-8")
            fixed += len(todo)

    # A row that finds no call is broken, not clean: the same defect as a
    # glob that silently matches nothing, one function at a time.
    missing = [f for f, k in calls.items() if k == 0]
    if missing:
        for f in missing:
            print(
                f"check_named_args: found no {f} call in the tracked"
                " .c/.h/.md files -- the scan is broken -- FAIL"
            )
        return 1
    if fixed:
        print(f"check_named_args: named {fixed} literal(s)")
    if left:
        print(
            "check_named_args: an integer-coded argument is a number -- FAIL"
        )
        for where, f in left:
            hint = f" (write {f.name})" if f.name else " (no such value)"
            print(f"  {where}: {f.func} {f.param} = {f.bare}{hint}")
        print(
            "\n  Pass DP_PSD_WINDOW_*, ACC_TRACE_* or DP_SPECTROGRAM_* by"
            "\n  name, or a variable that holds one; `make format` writes the"
            "\n  names. A deliberately invalid value goes in a named variable."
        )
        return 1
    print(
        f"check_named_args: OK -- {sum(calls.values())} call(s) in {files}"
        " file(s), every integer-coded argument passed by name"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
