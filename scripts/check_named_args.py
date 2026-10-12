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
which code. A DIRECT call whose named slot is, once comments, enclosing
parentheses and casts are stripped, an integer literal (decimal, octal, hex
or binary, any ``u``/``l`` suffix, either sign) or a character literal, is
refused. Comments inside the call are stepped over when it is split into
arguments, so a comma in ``1 /* Kaiser, beta 8 */`` cannot hide it. A cast
is recognised by a TYPE spelling (``int``, ``unsigned int``, ``size_t``,
any ``*_t``), never by "a name in parentheses": ``(base) + 1`` is a sum.
A call with a different argument count is a finding too, since a moved
signature would otherwise leave its row checking nothing. Prose is not a
call: ``dp_psd_create()``, ``dp_psd_create(1 << 62, ...)``, or a call
inside a string literal.

**Helpers, file by file.** A helper that FORWARDS its argument into a
constructor's gated slot -- a test's ``ref_psd (nfft, window, beta)``, the
certify harness's ``make (mode, nfft, hop, window)``, directly or through
another helper -- would hide a literal from the constructor's row, so
``HELPERS`` gives it a row for the one file that defines it (names like
``make`` are generic; #2191). A helper that only compares a window or mode
reaches no constructor and has no row. A definition or prototype, every
argument a declaration or a bare type, is not a call, and neither is a
member call (``ops->make``). **Not held:** a forwarding helper ``HELPERS``
does not list (found by hand; #2197), a second hop through a function
pointer or macro, or a value that never passes through a call slot -- a
variable initialised from a literal, a table cell, a loop bound.

**One declaration.** Each slot names an ``[[enum]]`` of just-makeit.toml,
and the names come from the C enum that carries it (``SSOT: enum=X``, held
to the manifest by ``check_wfm_enum_tables.py``'s rule 7), read with that
gate's own parser. So this file keeps no copy of any name or value.

**Registration-free.** It scans every tracked ``*.c``, ``*.h`` and ``*.md``
(``git ls-files``), so a new example, test, bench or doc page is checked the
moment it is committed. Calls are joined across lines, and a header comment's
``*`` continuation is stripped first, so a call wrapped inside an ``@code``
block is read whole. ``docs/c-api/`` is skipped: it is generated from the
headers this already scans. ``example-projects/`` is scanned, except its
window slots: those projects build against a RELEASED doppler, which has the
mode names but no ``DP_PSD_WINDOW_*`` yet (#2186).

**Fail-closed, per row.** Every function in ``TABLE`` must be called at least
once: a row whose function was renamed away would otherwise check nothing
and read as clean.

**What it does not refuse:** a variable initialised from a literal, or an
expression such as ``1 - 1``. A refusal test that needs an invalid value
(``-1``, 4) names it in a variable. The gate is for the slip, not the
determined.

**--fix** rewrites each refused literal to the name whose C value it is, in
place and by source position, so ``make format`` (which runs
``lint-named-args``) is the fixer the gate points at. It rewrites only
what it can PROVE: the argument, rewritten, must reduce to exactly the
name. A literal that is no value of its enum, or one it cannot prove, is
left alone and still refused.

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
    "dp_psd_kernel_init": (6, {2: ("window", "psd_window")}),
    "dp_acc_trace_create": (3, {1: ("mode", "acc_trace_mode")}),
}

_W = ("window", "psd_window")
_M = ("mode", "spectrogram_mode")
#: File-scoped rows (#2191): a helper that FORWARDS its argument into a
#: gated slot of a constructor -- directly, or through another helper here
#: -- hides a literal from that constructor's row, so it gets a row of its
#: own, for the one file that defines it (helper names like `make` are
#: generic). A helper that only COMPARES a window or mode against a name
#: reaches no constructor and has no row. Same shape as TABLE, keyed by the
#: file. Rows are found by hand (#2197 asks for discovery).
HELPERS: dict[str, dict[str, tuple[int, dict[int, tuple[str, str]]]]] = {
    "native/tests/test_spectrogram_core.c": {
        "ref_psd": (3, {1: _W}),
        "oneshot": (7, {0: _M, 3: _W}),
        "rows_are_the_psd": (7, {1: _M}),
        "sections": (1, {0: _M}),
    },
    "native/validation/spectrogram_certify.c": {
        "row_of": (3, {0: _W}),
        "make": (4, {0: _M, 3: _W}),
        "ref_psd": (2, {1: _W}),
        "oracle": (6, {0: _M, 3: _W}),
        "partition": (11, {0: _M, 3: _W}),
    },
    "native/benchmarks/bench_spectrogram_core.c": {
        "add": (8, {7: _M}),
    },
}

#: Tracked trees that are not this gate's: generated from what it scans.
SKIP = ("docs/c-api/",)

#: path prefix -> the enums whose slots it may still write as numbers. The
#: example projects build against a RELEASED doppler (uno-q pins 0.66.0),
#: which has the mode names but no DP_PSD_WINDOW_* yet (#2186); every other
#: slot there is checked.
EXEMPT_SLOTS = {"example-projects/": frozenset({"psd_window"})}


def _call_re(table: dict[str, object]) -> re.Pattern[str]:
    """A call of any function in `table`. Longest first, so
    `dp_psd_create_hop` is never read as `dp_psd_create`. A member call,
    `s.make (...)` or `ops->make (...)`, is another function."""
    return re.compile(
        r"(?<![\w.])(?<!->)("
        + "|".join(sorted(table, key=len, reverse=True))
        + r")\s*\("
    )


_CALL = _call_re(TABLE)
#: A parameter declaration: type words, any `*`, then a name, and an
#: optional array suffix -- `size_t nfft`, `const float _Complex *frame`,
#: `unsigned long long x`, `float *restrict out`. Where a call has an
#: expression, a declaration has words. A definition or prototype (every
#: argument one of these, or `_TYPE_ONLY`) is not a call, so it neither
#: counts toward its row nor stands in for a missing one.
_IDENT = r"[A-Za-z_]\w*"
_DECL = re.compile(rf"(?:{_IDENT}[\s*]+)+{_IDENT}(?:\s*\[[^\]]*\])?")
#: An unnamed prototype parameter: a type alone -- `size_t`, `void`,
#: `const float _Complex *`. Built from C's own type words and `*_t` only,
#: so a variable passed alone (`w`) is never one.
_TYPE_WORD = (
    r"(?:void|char|short|int|long|float|double|signed|unsigned|const"
    r"|volatile|restrict|_Bool|bool|_Complex|struct\s+"
    + _IDENT
    + r"|"
    + _IDENT
    + r"_t)"
)
_TYPE_ONLY = re.compile(rf"{_TYPE_WORD}(?:[\s*]+{_TYPE_WORD})*[\s*]*")

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
#: A cast is recognised by a TYPE spelling, never by "a name in parentheses":
#: `(base) + 1` is a sum, and reading `(base)` as a cast made it the literal
#: `+ 1` (and --fix wrote `(base) DP_PSD_WINDOW_KAISER`). A typedef'd integer
#: ends in `_t`; `(unsigned int)1` is a cast, which the old one-word rule
#: missed.
_TYPE = (
    r"(?:(?:const|volatile)\s+)*"
    r"(?:(?:(?:signed|unsigned|short|long|int|char)\b\s*)+"
    r"|(?:float|double|_Bool|bool|[A-Za-z_]\w*_t)\b)"
)
_CAST = re.compile(r"\(\s*" + _TYPE + r"\s*\)\s*(.+)", re.S)
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
    parentheses never close (a fragment cut off by the file's end).

    A comment inside the call is stepped over, not read: a comma in
    `1 /* Kaiser, beta 8 */` split the argument in two, the call then had
    the wrong count, and it was skipped as no call at all. A call inside a
    header's @code block carries no comment of its own, so stepping over
    `/* ... */` and `// ...` here cannot hide one."""
    depth, spans, a, i, n = 0, [], start + 1, start, len(text)
    while i < n:
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            if j < 0:
                return None
            i = j + 2
            continue
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
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
        i += 1
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
    """One refused literal, and the name that replaces it if there is one.

    `span` is set only when --fix can PROVE the rewrite: the argument,
    rewritten, reduces to exactly the name. Anything else is reported and
    left alone, so the fixer cannot write code that does not compile.
    """

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


def _fix_span(text: str, s: int, e: int, name: str) -> tuple[int, int] | None:
    """The span of the lone integer token in argument text[s:e], if replacing
    it with `name` leaves an argument that reduces to exactly `name`."""
    for t in _TOKEN.finditer(text, s, e):
        if t.group(2):
            a, b = t.span(2)
            if _bare(text[s:a] + name + text[b:e]) == name:
                return a, b
            return None
    return None


def check_text(
    text: str,
    names: dict[str, dict[int, str]],
    is_c: bool,
    exempt: frozenset[str] = frozenset(),
    helpers: dict[str, tuple[int, dict[int, tuple[str, str]]]] | None = None,
) -> tuple[dict[str, int], list[Finding]]:
    """Calls checked per function, and each finding: a literal in a named
    slot (not one of the `exempt` enums), or a call whose argument count is
    not its row's -- a signature change must not leave a stale row quietly
    checking nothing. A zero-argument mention is prose, not a call, and a
    definition or prototype (every argument a declaration) is not one
    either. `helpers` are this file's own rows, checked beside TABLE's."""
    table = {**TABLE, **(helpers or {})}
    scan = _STRING.sub(lambda m: " " * len(m.group(0)), text) if is_c else text
    hits = [(m, scan) for m in _CALL.finditer(scan)]
    if helpers:
        # A helper is file-local C, so a mention of one inside a comment is
        # prose (`the units sections (3, 9, 13)`), never a call: its rows
        # read the file with comments blanked. TABLE's rows cannot, because
        # a header's @code block, which they must read, is a comment.
        code = _COMMENT.sub(lambda m: " " * len(m.group(0)), scan)
        hits += [(m, code) for m in _call_re(helpers).finditer(code)]
        hits.sort(key=lambda h: h[0].start())
    calls: dict[str, int] = {}
    found: list[Finding] = []
    for m, src in hits:
        func = m.group(1)
        nargs, slots = table[func]
        got = _args(src, m.end() - 1)
        if got is None:
            continue
        spans = got[0]
        bares = [_bare(text[s:e]) for s, e in spans]
        # Prose, not a call: `dp_psd_create()`, or `dp_psd_create(1 << 62,
        # ...)` with an ellipsis standing for the rest.
        if bares == [""] or "..." in bares:
            continue
        if all(_DECL.fullmatch(b) or _TYPE_ONLY.fullmatch(b) for b in bares):
            continue
        line = text.count("\n", 0, m.start()) + 1
        if len(spans) != nargs:
            found.append(
                Finding(
                    line,
                    func,
                    "arity",
                    f"{len(spans)} argument(s), its row says {nargs}",
                    None,
                    None,
                )
            )
            continue
        calls[func] = calls.get(func, 0) + 1
        for idx, (param, enum) in slots.items():
            if enum in exempt:
                continue
            s, e = spans[idx]
            bare = _bare(text[s:e])
            if not (_INT.fullmatch(bare) or _CHAR.fullmatch(bare)):
                continue
            name, span = None, None
            if _INT.fullmatch(bare):
                name = names.get(enum, {}).get(_value(bare))
                if name:
                    span = _fix_span(text, s, e, name)
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
    rows = [
        *TABLE.values(),
        *(r for h in HELPERS.values() for r in h.values()),
    ]
    for enum in sorted({e for _, s in rows for _, e in s.values()}):
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
    helper_calls = {(p, f): 0 for p, rows in HELPERS.items() for f in rows}
    files, fixed, left = 0, 0, []
    for rel in tracked(root):
        path = root / rel
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        exempt = frozenset().union(
            *(e for p, e in EXEMPT_SLOTS.items() if rel.startswith(p))
        )
        helpers = HELPERS.get(rel)
        n, found = check_text(
            text, names, not rel.endswith(".md"), exempt, helpers
        )
        if n:
            files += 1
            for f, k in n.items():
                if helpers and f in helpers:
                    helper_calls[(rel, f)] += k
                else:
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

    if fixed:
        print(f"check_named_args: named {fixed} literal(s)")
    # The findings print first: an arity change can be WHY a row below
    # found no call, and the cause must not be hidden behind its effect.
    if left:
        print(
            "check_named_args: an integer-coded argument is a number -- FAIL"
        )
        for where, f in left:
            if f.param == "arity":
                print(f"  {where}: {f.func} called with {f.bare}")
                continue
            if not f.name:
                hint = " (no such value)"
            elif args.fix:
                hint = f" ({f.name}, but not a lone literal: edit it by hand)"
            else:
                hint = f" (write {f.name})"
            print(f"  {where}: {f.func} {f.param} = {f.bare}{hint}")
        print(
            "\n  Pass DP_PSD_WINDOW_*, ACC_TRACE_* or DP_SPECTROGRAM_* by"
            "\n  name, or a variable that holds one; `make format` writes the"
            "\n  names. A deliberately invalid value goes in a named variable."
            "\n  A call whose argument count is not its TABLE or HELPERS"
            "\n  row's means the signature moved: fix the call, or the row."
        )
    # A row that finds no call is broken, not clean: the same defect as a
    # glob that silently matches nothing, one function at a time.
    missing = [f for f, k in calls.items() if k == 0]
    for f in missing:
        print(
            f"check_named_args: found no {f} call in the tracked"
            " .c/.h/.md files -- the scan is broken -- FAIL"
        )
    # A helper row is held the same way, in its own file. A rename needs the
    # row renamed (dropping it would un-gate the helper); only a helper that
    # is gone takes its row with it.
    lost = [(p, f) for (p, f), k in helper_calls.items() if k == 0]
    for p, f in lost:
        print(
            f"check_named_args: found no call of the helper {f} in {p}"
            " -- rename the row, or drop it if the helper is gone -- FAIL"
        )
    if left or missing or lost:
        return 1
    print(
        f"check_named_args: OK -- {sum(calls.values())} call(s) and"
        f" {sum(helper_calls.values())} helper call(s) in {files} file(s),"
        " every integer-coded argument passed by name"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
