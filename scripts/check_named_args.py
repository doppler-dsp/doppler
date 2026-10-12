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

**Helpers, discovered.** A helper that FORWARDS its argument into a
constructor's gated slot -- a test's ``ref_psd (nfft, window, beta)``, the
certify harness's ``make (mode, nfft, hop, window)``, directly or through
another helper -- would hide a literal from the constructor's row (#2191).
``discover`` finds them by data flow (#2197): a ``static`` function one of
whose parameters is passed bare into a gated slot of a TABLE call, or of
another helper in the file, gets that row, for its own file (names like
``make`` are generic). A parameter that reaches two enums keeps both, so a
literal at its call is named only where they agree (#2227). A variadic
helper's ``...`` is not a parameter: its named parameters are the fewest
arguments a call may pass (#2227). A local that re-declares a parameter
(``for (int w = 0; ...)``) ends it: from that declaration on, the name is
the local's, so it teaches no helper (#2227). A parameter only COMPARED
with an enumerator of an enum TABLE gates (``mode == DP_SPECTROGRAM_POWER``,
either way round, ``!=``, or a ``switch`` and its ``case`` labels) is a slot
of that enum too: a literal at its call is the #2097 renumbering hazard all
the same (#2227). A definition is read in the shapes this tree writes:
``static`` at the start of a line or indented, after any upper-case macro
(``JM_HOT``), ``[[attribute]]`` or ``__attribute__((...))``, with the return
type, qualifiers and attributes on that line or any line before the name.
Its rows read the file with comments blanked. A definition or prototype,
every argument a declaration or a bare type, is not a call, and neither is a
member call (``ops->make``).
**Not held:** a non-``static`` forwarding function (a library's own entry
points are TABLE rows); a ``static inline`` forwarder in a header
(discovery reads ``.c`` files only); a K&R definition (its parameters are
declared after the list); a parameter forwarded inside an expression
(``w + 0``) or through a local variable; a parameter compared with a
literal (``mode == 0``), through a macro, or with an enumerator of an enum
no TABLE slot uses; a parameter forwarded after the block of a local that
shadowed it closes, or one shadowed by a declarator after a comma
(``int a, w;``) or a typedef not spelled ``*_t`` (#2227); a function pointer
or macro;
a value that never passes through a call slot -- a variable initialised
from a literal, a table cell, a loop bound.

**One declaration.** Each slot names an ``[[enum]]`` of just-makeit.toml,
and the names come from the C enum that carries it (``SSOT: enum=X``, held
to the manifest by ``check_wfm_enum_tables.py``'s rule 7), read with that
gate's own parser. A view (``carrier_acq_window``) has its parent's names,
restricted to the view, so ``--fix`` names only what the constructor takes
and a value outside the view is reported as no such value (#2194). So this
file keeps no copy of any name or value.

**Registration-free.** It scans every tracked ``*.c``, ``*.h`` and ``*.md``
(``git ls-files``), so a new example, test, bench or doc page is checked the
moment it is committed. Calls are joined across lines, and a header comment's
``*`` continuation is stripped first, so a call wrapped inside an ``@code``
block is read whole. ``docs/c-api/`` is skipped: it is generated from the
headers this already scans. ``example-projects/`` is scanned like the rest:
those projects build against a RELEASED doppler, and v0.67.0 carries the
``DP_PSD_WINDOW_*`` names, so their window slots are held too (#2186).

**Fail-closed.** Every function in ``TABLE`` must be called at least once: a
row whose function was renamed away would otherwise check nothing and read
as clean. A discovered helper has no row to go stale, but discovery can stop
SEEING one -- a definition in a shape it does not read is dropped, and its
calls pass unchecked. A ``static`` body whose braces do not close (an
``#if`` and its ``#else`` each opening one) is a finding: read to the
file's end, it would take in every function below it and teach a false
helper (#2227). Otherwise the gate fails only when it discovers no helper
anywhere; it does not know which helpers the tree should have. Its test
does: ``test_the_real_trees_helpers_are_discovered`` pins each real helper
(file, name, argument count, slots and enums) as a subset, so a lost or
changed helper is red there, and a new one needs no edit.

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

from check_wfm_enum_tables import enum_names

ROOT = Path(__file__).resolve().parent.parent

#: function -> (argument count, {argument index: (parameter, [[enum]])}).
#: A discovered variadic helper's count is negative: at least that many.
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
    "dp_carrier_acq_create": (13, {4: ("window", "carrier_acq_window")}),
    "dp_psd_kernel_init": (6, {2: ("window", "psd_window")}),
    "dp_acc_trace_create": (3, {1: ("mode", "acc_trace_mode")}),
}

#: Tracked trees that are not this gate's: generated from what it scans.
SKIP = ("docs/c-api/",)


def _fits(count: int, nargs: int) -> bool:
    """Whether a call with `count` arguments is a call of a row of `nargs`:
    exactly that many, or for a variadic helper's row (negative) at least
    its named parameters. `f (int w, ...)` is -1, so `f (w)` and
    `f (w, "x", 3)` are both its calls (#2227).

    >>> _fits(2, 2), _fits(3, 2), _fits(1, -1), _fits(3, -1), _fits(1, -2)
    (True, False, True, True, False)
    """
    return count == nargs or 0 < -nargs <= count


def _says(nargs: int) -> str:
    """A row's argument count as an arity finding states it."""
    return f"at least {-nargs}" if nargs < 0 else str(nargs)


def _call_re(table: dict[str, object]) -> re.Pattern[str]:
    """A call of any function in `table`. Longest first, so
    `dp_psd_create_hop` is never read as `dp_psd_create`. A member call,
    `s.make (...)` or `ops->make (...)`, is another function. A comment
    between the name and its `(` is stepped over (#2198) -- a TABLE row's
    concern: a helper's rows already read the file with comments blanked.
    The comment is matched with a tempered dot, never a lazy `.*?`, which
    re.S lets cross into the next comment and backtrack."""
    return re.compile(
        r"(?<![\w.])(?<!->)("
        + "|".join(sorted(table, key=len, reverse=True))
        + r")\s*(?:/\*(?:(?!\*/).)*\*/\s*|//[^\n]*\n\s*)*\(",
        re.S,
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
#: A comment, or a C string or character literal, whichever STARTS first.
#: Blanking them in this one left-to-right pass, rather than strings and
#: then comments, is what keeps a quote inside a comment (`/* 5" */`) from
#: opening a "string" that swallows the comment's `*/`, and a `/*` inside a
#: string from opening a comment (#2227). A `printf ("dp_psd_create(%zu)")`
#: is then not a call either.
_LEXEME = re.compile(
    r"(?P<comment>/\*.*?\*/|//[^\n]*)"
    r'|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])\'',
    re.S,
)


def _blank_lexemes(text: str, comments: bool) -> str:
    """`text` with every string and character literal blanked to spaces, and
    every comment too when `comments`, in one pass (same offsets kept).

    TABLE's calls are read with comments kept, because a header's @code
    block is a comment; a helper's calls, and discovery, with them blanked.

    >>> _blank_lexemes('/* 5" */ f ("x");', comments=False)
    '/* 5" */ f (   );'
    >>> _blank_lexemes('/* 5" */ f ("x");', comments=True)
    '         f (   );'
    """

    def sub(m: re.Match[str]) -> str:
        if m.group("comment") is not None and not comments:
            return m.group(0)
        return " " * len(m.group(0))

    return _LEXEME.sub(sub, text)


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
        note: str | None = None,
    ) -> None:
        self.line = line
        self.func = func
        self.param = param
        self.bare = bare
        self.name = name
        self.span = span
        self.note = note  # why no name is offered, when it is not "none"


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


#: An attribute: `[[maybe_unused]]`, or `__attribute__((x))` with at most
#: one level of parentheses inside (`__attribute__((aligned (16)))`).
_ATTR = r"(?:\[\[[^\]]*\]\]|__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\))"

#: A file-local function's definition, GNU style or not: `static` at the
#: start of a line or indented, after any upper-case macros (`JM_HOT`) and
#: attributes; then the return type, qualifiers and attributes across any
#: number of lines; then the name, never `__attribute__` itself (it is
#: followed by `(` too), and `(`. Nothing in between may be `;`, a brace or
#: a parenthesis outside an attribute, so a call or a statement ends it.
#: Whether a `{` follows the parameter list, making it a definition and not
#: a prototype, is the caller's check.
_STATIC_DEF = re.compile(
    r"^[ \t]*"
    rf"(?:(?:{_ATTR}|[A-Z_][A-Z0-9_]*)\s+)*"
    r"static\b"
    rf"(?:{_ATTR}|[^;{{}}()])*?"
    r"(?P<name>(?!__attribute__\b)[A-Za-z_]\w*)\s*\(",
    re.MULTILINE,
)


def _brace_end(code: str, j: int) -> int | None:
    """The offset of the `}` closing the `{` at `j` (comments and strings
    already blanked), or None when it never closes: an `#if` and its
    `#else` that each open a brace, read together, leave one open (#2227).

    >>> _brace_end("{ { } }", 0), _brace_end("{ {\\n{\\n} }", 0)
    (6, None)
    """
    depth = 0
    for i in range(j, len(code)):
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return None


def _shadowed(code: str, name: str, body: int, at: int) -> bool:
    """Whether the body opened at `body` declares a local `name` before
    `at`. From that declaration on, `name` is the local and not the
    parameter (#2227). A declaration is read as a type word, any `*`, then
    the name, so `for (int w = 0; ...)` and `size_t *w;` count, and a use
    (`w + 1`, `(int) w`) does not.

    >>> _shadowed("{ for (int w = 0;;) f (w); }", "w", 0, 20)
    True
    >>> _shadowed("{ f (w); int w; }", "w", 0, 6)
    False
    """
    decl = rf"\b{_TYPE_WORD}[\s*]+{re.escape(name)}\b"
    return re.search(decl, code[body:at]) is not None


def _param_name(param: str) -> str | None:
    """The name a parameter declaration binds, or None for `void`, `...`
    and an unnamed one (a bare type)."""
    p = re.sub(r"\[[^\]]*\]", "", param).strip()
    ids = re.findall(r"[A-Za-z_]\w*", p)
    return ids[-1] if len(ids) >= 2 else None


def _add_slot(
    rows: dict[str, tuple[int, dict[int, tuple[str, str]]]],
    name: str,
    arity: int,
    p: int,
    label: str,
    enum: str,
) -> bool:
    """Give helper `name`'s parameter `p` the slot (`label`, `enum`), and
    say whether its row changed. A parameter that reaches two enums (a
    window passed to PSD and to carrier_acq's view) keeps both, "a|b", so a
    literal at its call is named only where they agree (#2227).

    >>> rows = {}
    >>> _add_slot(rows, "f", 1, 0, "w", "b")
    True
    >>> _add_slot(rows, "f", 1, 0, "w", "a")
    True
    >>> _add_slot(rows, "f", 1, 0, "w", "b"), rows
    (False, {'f': (1, {0: ('w', 'a|b')})})
    """
    row = rows.setdefault(name, (arity, {}))
    old = row[1].get(p)
    if old is None:
        row[1][p] = (label, enum)
        return True
    if enum in old[1].split("|"):
        return False
    both = set(old[1].split("|")) | {enum}
    row[1][p] = (old[0], "|".join(sorted(both)))
    return True


#: A `case` label of a switch.
_CASE = re.compile(r"\bcase\s+([A-Za-z_]\w*)\s*:")


def _compared(
    code: str, param: str, body: int, end: int, gated: dict[str, str]
) -> set[str]:
    """The gated enums whose enumerators `param` is COMPARED with in the
    body code[body:end]: `param == X` or `X == param`, `!=` either way, or a
    `switch (param)` whose `case` labels are X (#2227). A member (`s->mode`,
    `s.mode`) is not the parameter, and neither is a name a local re-declared
    before the compare.

    >>> g = {"DP_SPECTROGRAM_DB": "spectrogram_mode"}
    >>> _compared("{ return DP_SPECTROGRAM_DB != m; }", "m", 0, 99, g)
    {'spectrogram_mode'}
    >>> _compared("{ return s->m == DP_SPECTROGRAM_DB; }", "m", 0, 99, g)
    set()
    >>> sw = "{ switch (m) { case DP_SPECTROGRAM_DB: ; } }"
    >>> _compared(sw, "m", 0, 99, g)
    {'spectrogram_mode'}
    """
    p = re.escape(param)
    me = rf"(?<![\w.])(?<!->){p}\b(?!\s*(?:->|\.|\[))"
    cmp = re.compile(
        rf"{me}\s*[!=]=\s*([A-Za-z_]\w*)\b|\b([A-Za-z_]\w*)\s*[!=]=\s*{me}"
    )
    found = set()
    for m in cmp.finditer(code, body, end):
        x = m.group(1) or m.group(2)
        if x in gated and not _shadowed(code, param, body, m.start()):
            found.add(gated[x])
    for m in re.compile(rf"\bswitch\s*\(\s*{p}\s*\)\s*\{{").finditer(
        code, body, end
    ):
        close = _brace_end(code, m.end() - 1)
        if close is None or _shadowed(code, param, body, m.start()):
            continue
        for c in _CASE.finditer(code, m.end(), close):
            if c.group(1) in gated:
                found.add(gated[c.group(1)])
    return found


def discover(
    text: str,
    gated: dict[str, str] | None = None,
) -> tuple[
    dict[str, tuple[int, dict[int, tuple[str, str]]]], list[tuple[int, str]]
]:
    """The file's forwarding helpers, found by data flow (#2197), and the
    (line, name) of each `static` definition whose body it cannot read.

    A `static` function is a helper when one of its parameters is passed,
    bare, into a gated slot of a TABLE call, or of another helper in the
    same file (to a fixpoint, so a helper that calls a helper is found). Its
    row is that parameter's position, the slot's name and the slot's enum.
    A parameter COMPARED with an enumerator of `gated` (`mode ==
    DP_SPECTROGRAM_POWER`, or a `switch` on it) is a slot of that enum,
    named for the parameter (#2227); a parameter used inside an expression
    (`w + 0`) is not forwarded bare and is not followed. Only `static`
    functions: a helper is file-local, so its row checks its own file's
    calls.

    A body whose braces never close is not read at all: taken to the end
    of the file, it would take in every function below it and their calls,
    and teach a false helper (#2227). It is returned for the caller to
    report, so the gate fails on it rather than reading past it.
    """
    code = _blank_lexemes(text, comments=True)
    defs = []
    unread: list[tuple[int, str]] = []
    for m in _STATIC_DEF.finditer(code):
        got = _args(code, m.end() - 1)
        if got is None:
            continue
        spans, end = got
        j = end
        while j < len(code) and code[j].isspace():
            j += 1
        if j >= len(code) or code[j] != "{":
            continue  # a prototype, not a definition
        params = [_param_name(code[a:b]) for a, b in spans]
        # `...` is no parameter: a variadic row counts its named ones as a
        # minimum, so a call with more arguments is still its call (#2227).
        arity = len(params)
        if code[spans[-1][0] : spans[-1][1]].strip() == "...":
            params.pop()
            arity = -len(params)
        k = _brace_end(code, j)
        if k is None:
            line = code.count("\n", 0, m.start("name")) + 1
            unread.append((line, m.group("name")))
            continue
        defs.append((m.group("name"), params, arity, j, k))
    rows: dict[str, tuple[int, dict[int, tuple[str, str]]]] = {}
    # A compare is a slot of its own: the seed the fixpoint below forwards
    # from, so a helper that passes a parameter into a COMPARING helper is
    # found too (`rows_are_the_psd` and `oracle` into `ref_row`).
    for name, params, arity, j, k in defs:
        for p, label in enumerate(params):
            if label:
                for enum in sorted(_compared(code, label, j, k, gated or {})):
                    _add_slot(rows, name, arity, p, label, enum)
    changed = True
    while changed:
        changed = False
        table = {**TABLE, **rows}
        call = _call_re(table)
        for name, params, arity, j, k in defs:
            for cm in call.finditer(code, j, k):
                func = cm.group(1)
                if func == name:
                    continue
                nargs, slots = table[func]
                got = _args(code, cm.end() - 1)
                if got is None or not _fits(len(got[0]), nargs):
                    continue
                for idx, (label, enum) in slots.items():
                    a, b = got[0][idx]
                    arg = _bare(code[a:b])
                    if (
                        arg
                        and arg in params
                        and not _shadowed(code, arg, j, cm.start())
                    ):
                        p = params.index(arg)
                        if _add_slot(rows, name, arity, p, label, enum):
                            changed = True
    return rows, unread


def check_text(
    text: str,
    names: dict[str, dict[int, str]],
    is_c: bool,
    helpers: dict[str, tuple[int, dict[int, tuple[str, str]]]] | None = None,
) -> tuple[dict[str, int], list[Finding]]:
    """Calls checked per function, and each finding: a literal in a named
    slot, or a call whose argument count is not its row's -- a signature
    change must not leave a stale row quietly checking nothing. A
    zero-argument mention is prose, not a call, and a definition or
    prototype (every argument a declaration) is not one either. `helpers`
    are this file's own rows, checked beside TABLE's."""
    table = {**TABLE, **(helpers or {})}
    scan = _blank_lexemes(text, comments=False) if is_c else text
    hits = [(m, scan) for m in _CALL.finditer(scan)]
    if helpers:
        # A helper is file-local C, so a mention of one inside a comment is
        # prose (`the units sections (3, 9, 13)`), never a call: its rows
        # read the file with comments blanked. TABLE's rows cannot, because
        # a header's @code block, which they must read, is a comment.
        code = _blank_lexemes(text, comments=True)
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
        if not _fits(len(spans), nargs):
            found.append(
                Finding(
                    line,
                    func,
                    "arity",
                    f"{len(spans)} argument(s), its row says {_says(nargs)}",
                    None,
                    None,
                )
            )
            continue
        calls[func] = calls.get(func, 0) + 1
        for idx, (param, enum) in slots.items():
            s, e = spans[idx]
            bare = _bare(text[s:e])
            if not (_INT.fullmatch(bare) or _CHAR.fullmatch(bare)):
                continue
            name, span, note = None, None, None
            if _INT.fullmatch(bare):
                # A discovered parameter can feed two enums ("a|b"): a
                # literal has a name only when every one of them agrees.
                each = {
                    e: names.get(e, {}).get(_value(bare))
                    for e in enum.split("|")
                }
                if len(set(each.values())) == 1:
                    name = next(iter(each.values()))
                else:
                    note = "feeds " + ", ".join(
                        f"{e} ({n or 'no such value'})"
                        for e, n in sorted(each.items())
                    )
                if name:
                    span = _fix_span(text, s, e, name)
            found.append(Finding(line, func, param, bare, name, span, note))
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
    """value -> name for every [[enum]] a slot uses, from its C enum. A view
    (carrier_acq_window) has its parent's names, restricted to the view, so
    a value outside it has none (#2194)."""
    by_enum, errs = enum_names(root)
    # A discovered helper's slots carry TABLE's enums, so TABLE names them all.
    for enum in sorted({e for _, s in TABLE.values() for _, e in s.values()}):
        if enum not in by_enum:
            errs.append(
                f"no C enum carries `SSOT: enum={enum}`, so a literal in its"
                " slots has no name to be checked or fixed against"
            )
    return by_enum, errs


def _enumerators(names: dict[str, dict[int, str]]) -> dict[str, str]:
    """enumerator -> the gated enum it belongs to, for the compares that
    discovery reads (#2227). Only TABLE's enums are gated, so a compare with
    any other enum's enumerator (`type == WFM_SYNTH_CHIRP`) is no slot. A
    view (carrier_acq_window) shares its parent's names; a compare with one
    says nothing about the view, so the name goes to the enum with the most
    names, the parent.

    >>> m = _enumerators(
    ...     {
    ...         "psd_window": {0: "H", 3: "R"},
    ...         "carrier_acq_window": {0: "H"},
    ...         "spectrogram_mode": {0: "P"},
    ...         "other": {0: "O"},
    ...     }
    ... )
    >>> m["H"], m["R"], m["P"], "O" in m
    ('psd_window', 'psd_window', 'spectrogram_mode', False)
    """
    gated = {e for _, s in TABLE.values() for _, e in s.values()}
    out: dict[str, str] = {}
    # Smallest first, so a parent, sorted after its view, has the last word.
    for enum in sorted(gated, key=lambda e: (len(names.get(e, {})), e)):
        for name in names.get(enum, {}).values():
            out[name] = enum
    return out


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
    gated = _enumerators(names)

    calls: dict[str, int] = dict.fromkeys(TABLE, 0)
    helper_calls: dict[tuple[str, str], int] = {}
    discovered = 0
    files, fixed, left = 0, 0, []
    unread: list[tuple[str, str]] = []
    for rel in tracked(root):
        path = root / rel
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        helpers = None
        if rel.endswith(".c"):
            helpers, lost = discover(text, gated)
            unread += [(f"{rel}:{line}", name) for line, name in lost]
        if helpers:
            discovered += len(helpers)
        n, found = check_text(text, names, not rel.endswith(".md"), helpers)
        if n:
            files += 1
            for f, k in n.items():
                if helpers and f in helpers:
                    helper_calls[(rel, f)] = helper_calls.get((rel, f), 0) + k
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
            if f.note:
                hint = f" ({f.note}: name it by hand)"
            elif not f.name:
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
            "\n  A call whose argument count is not its TABLE row's, or a"
            "\n  discovered helper's, means the signature moved: fix the call"
            "\n  (or the TABLE row)."
        )
    # A body discovery cannot read could hold a helper, or teach a false one
    # if read past its end, so it fails like a row that finds no call.
    if unread:
        print("check_named_args: a static body cannot be read -- FAIL")
        for where, name in unread:
            print(
                f"  {where}: {name}: its braces do not close (an #if and its"
                " #else each opening one?), so discovery cannot read it;"
                " open the brace once, outside the #if"
            )
    # A row that finds no call is broken, not clean: the same defect as a
    # glob that silently matches nothing, one function at a time.
    missing = [f for f, k in calls.items() if k == 0]
    for f in missing:
        print(
            f"check_named_args: found no {f} call in the tracked"
            " .c/.h/.md files -- the scan is broken -- FAIL"
        )
    # Discovery is held the same way: a tree with forwarding helpers in it
    # where it finds none has a broken discovery, not a clean tree.
    blind = discovered == 0
    if blind:
        print(
            "check_named_args: discovered no forwarding helper in any tracked"
            " .c file -- the discovery is broken -- FAIL"
        )
    if left or unread or missing or blind:
        return 1
    print(
        f"check_named_args: OK -- {sum(calls.values())} call(s) and"
        f" {sum(helper_calls.values())} call(s) of {discovered} discovered"
        f" helper(s) in {files} file(s),"
        " every integer-coded argument passed by name"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
