#!/usr/bin/env python3
"""dB rows are asked for by name: no Spectrogram mode is written as a number.

The owner's rule on #1968: power rows are the default, and "every caller
that wants dB rows names it". The Spectrogram's C constructor takes its
mode as a plain ``int``, so the compiler accepts ``0`` and ``1`` as
readily as ``DP_SPECTROGRAM_POWER`` and ``DP_SPECTROGRAM_DB``, and a number
says nothing about which mode the writer meant. Round 1 of #2097 found the
values themselves had been the wrong way round for the zero value; a call
written as a number would have changed meaning silently when they were
swapped. So a call passes its mode by name, or as a variable that holds
one, and this gate holds that.

**Registration-free.** It scans every tracked ``*.c``, ``*.h`` and ``*.md``
(``git ls-files``), so a new example, test, bench or doc page is checked the
moment it is committed, with no list to add it to. Calls are joined across
lines, and a header comment's ``*`` continuation is stripped first, so a
call wrapped inside an ``@code`` block is read whole.

**What it refuses.** A call to ``dp_spectrogram_create`` with five
arguments whose fifth, after any enclosing parentheses and casts, is an
integer literal (decimal, hex or binary, any ``u``/``l`` suffix, either
sign) or a character literal. A mention with any other number of arguments
(prose's ``dp_spectrogram_create()``) is not a call and is skipped. The
declaration's ``int mode`` passes because it is a name.

**What it does not refuse:** a variable initialised from a literal, or an
expression such as ``1 - 1``. Those are deliberate, and the reviewer sees
them. The gate is for the slip, not the determined.

Usage
-----
    python3 scripts/check_spectrogram_mode.py           # exit 1 on a literal
    python3 scripts/check_spectrogram_mode.py --root D  # scan another tree
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: The constructor whose mode is checked, and the mode's argument index.
FUNCTION = "dp_spectrogram_create"
NARGS = 5
MODE_ARG = 4

_CALL = re.compile(r"(?<![\w.])" + FUNCTION + r"\s*\(")

#: A line break inside a call, with a comment's continuation prefix (`*`,
#: never the `*/` that closes it, or `//`) removed, so a call wrapped in a
#: header's @code block reads as one line.
_BREAK = re.compile(r"\n[ \t]*(?:\*(?!/)|//)?")

_INT = re.compile(r"[+-]?\s*(?:0[xX][0-9a-fA-F]+|0[bB][01]+|\d+)[uUlL]*")
_CHAR = re.compile(r"'(?:\\.|[^'\\])'")
_CAST = re.compile(r"\(\s*(?:const\s+)?[A-Za-z_]\w*\s*\)\s*(.+)", re.S)


def _args(text: str, start: int) -> tuple[list[str], int] | None:
    """The top-level arguments of the call whose `(` is at `start`.

    Returns them with the offset just past the closing `)`, or None when
    the parentheses never close (a fragment cut off by the file's end).
    """
    depth, args, cur = 0, [], []
    for i in range(start, len(text)):
        ch = text[i]
        if ch in "([{":
            depth += 1
            if depth == 1:
                continue
        elif ch in ")]}":
            depth -= 1
            if depth == 0:
                args.append("".join(cur))
                return args, i + 1
        elif ch == "," and depth == 1:
            args.append("".join(cur))
            cur = []
            continue
        cur.append(ch)
    return None


def _bare(arg: str) -> str:
    """The argument with its enclosing parentheses and casts removed."""
    a = _BREAK.sub(" ", arg).strip()
    while True:
        if a.startswith("("):
            # one pair enclosing the whole argument, not `(a) + (b)`
            inner = _args(a, 0)
            if inner and inner[1] == len(a) and len(inner[0]) == 1:
                a = inner[0][0].strip()
                continue
        m = _CAST.fullmatch(a)
        if m:
            a = m.group(1).strip()
            continue
        return a


def _is_literal(arg: str) -> bool:
    a = _bare(arg)
    return bool(_INT.fullmatch(a) or _CHAR.fullmatch(a))


def check_text(text: str) -> tuple[int, list[tuple[int, str]]]:
    """Calls checked, and (line, mode) for each whose mode is a literal."""
    calls, bad = 0, []
    for m in _CALL.finditer(text):
        got = _args(text, m.end() - 1)
        if got is None or len(got[0]) != NARGS:
            continue
        calls += 1
        mode = got[0][MODE_ARG]
        if _is_literal(mode):
            line = text.count("\n", 0, m.start()) + 1
            bad.append((line, _bare(mode)))
    return calls, bad


def tracked(root: Path) -> list[Path]:
    """Every tracked C source, C header and Markdown page under `root`."""
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--", "*.c", "*.h", "*.md"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return [root / p for p in out.split("\0") if p]


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    # A gate that can only run against the real tree cannot be sabotaged
    # without breaking doppler, so its test points it at a seeded one.
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args(argv[1:]).root.resolve()

    calls, files, bad = 0, 0, []
    for path in tracked(root):
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        n, found = check_text(text)
        if n:
            files += 1
            calls += n
        rel = path.relative_to(root)
        bad += [(f"{rel}:{line}", mode) for line, mode in found]

    # A scan that finds no call at all is broken, not clean: the same
    # defect as a glob that silently matches nothing.
    if calls == 0:
        print(
            f"check_spectrogram_mode: found no {FUNCTION} call in the "
            "tracked .c/.h/.md files — the scan is broken — FAIL"
        )
        return 1
    if bad:
        print(
            f"check_spectrogram_mode: {FUNCTION}'s mode is a number, not a "
            "name — FAIL"
        )
        for where, mode in bad:
            print(f"  {where}: mode = {mode}")
        print(
            "\n  Pass DP_SPECTROGRAM_POWER (the default) or DP_SPECTROGRAM_DB"
            "\n  by name, or a variable that holds one: dB rows are asked for"
            "\n  by name (#1968), and a number hides which mode was meant."
        )
        return 1
    print(
        f"check_spectrogram_mode: OK — {calls} call(s) in {files} file(s), "
        "every mode passed by name"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
