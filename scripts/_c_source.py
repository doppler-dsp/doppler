r"""Reading C as a scanner must: comments blanked, string literals whole.

A gate that looks for a spelling in C has two questions to answer first, and
both have been answered wrong here more than once. The first is what is a
comment: documentation that quotes the forbidden spelling must not trip the
gate, and a ``/*`` inside ``"a/*b"`` must not blank real code up to the
next ``*/``. The second is what is a string: a format the compiler joins
from ``"%s" ".e"`` is one literal, and an apostrophe in an ``#if 0`` block
must not open a character literal that swallows the code after it.

This module is where those answers live for the gates that use it,
``check_tests_ssot.py`` and ``check_tlm_name_join.py``.
:func:`strip_comments` came from ``check_tests_ssot.py``, where review
found the literal-blind version's false negative.
``check_tlm_name_join.py`` had grown a second copy without the guard
against an unterminated literal, and that copy missed a bare join below an
``#if 0`` block that contained "it's" (#1944 review). Other scripts here
still strip comments with a regex of their own, which knows nothing of
literals; #1984 moves them onto this module.

>>> strip_comments("int a; /* x */ int b; // y\nint c;")
'int a;         int b;     \nint c;'
>>> strip_comments('"a/*b" /* c */ "d*/e"')
'"a/*b"         "d*/e"'
>>> string_literals('f ("%s"\n   ".e", p);')
[(1, '%s.e')]
>>> string_literals('#if 0\nit\'s\n#endif\nx ("%s.%s");')
[(4, '%s.%s')]
"""

from __future__ import annotations


def strip_comments(text: str) -> str:
    """Blank out C comments, preserving line numbering.

    Required, not tidiness: `dp_rng_test.h` documents the broken generator it
    replaced by QUOTING it, and `test_costas_core.c` explains in prose what it
    no longer does. A scanner that reads comments would fire on the
    documentation of the very rule it enforces — the failure mode where
    describing a detector's target blinds or trips the detector.

    String- and char-literal aware, which the first version was not. A regex
    split treats the `/*` inside `"a/*b"` as opening a comment and blanks
    everything up to the next `*/` in any later literal — taking real code
    with it and reporting zero violations for the span. A false NEGATIVE, in
    a gate whose entire value is being absolute, triggered by a test gaining
    a URL or a format string. Found by review, not by the sabotage battery,
    which only ever fed it well-formed code.
    """
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":  # literal: copied out verbatim
            quote, j = c, i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == quote:
                    j += 1
                    break
                if text[j] == "\n":  # unterminated; do not run away
                    break
                j += 1
            out.append(text[i:j])
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def string_literals(code: str) -> list[tuple[int, str]]:
    """Every string literal in ``code``, adjacent ones joined, with its line.

    ``code`` should already have been through :func:`strip_comments`. A
    literal is taken whole with its escapes as written, and the ones C
    concatenates (separated only by whitespace) are returned as one, on the
    line the first begins. Character literals are skipped, so ``'"'`` opens
    no string. A literal that reaches the end of its line unterminated stops
    there, as in :func:`strip_comments`, so one stray quote cannot consume
    the rest of the file.
    """
    out: list[tuple[int, str]] = []
    i, n, line = 0, len(code), 1
    joining = False
    while i < n:
        c = code[i]
        if c in "\"'":
            j = i + 1
            while j < n and code[j] != c and code[j] != "\n":
                j += 2 if code[j] == "\\" else 1
            body = code[i + 1 : min(j, n)]
            start = line
            line += body.count("\n")  # a backslash-newline continuation
            i = j + 1 if j < n and code[j] == c else j
            if c == "'":
                joining = False
                continue
            if joining:
                out[-1] = (out[-1][0], out[-1][1] + body)
            else:
                out.append((start, body))
            joining = True
            continue
        if c == "\n":
            line += 1
        elif not c.isspace():
            joining = False
        i += 1
    return out
