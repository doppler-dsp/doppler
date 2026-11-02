"""How a validation finding is named and cited: the one declaration.

A report numbers its findings by position -- ``F1`` for the first
``Report.find()`` -- so a number typed into prose re-points the moment a
finding before it is dropped or reordered: #2056 dropped one PSD finding
and left five citations pointing at the wrong one, none caught (#2059).
So a finding is named by a KEY, and its number is only ever derived.

Two consumers read these patterns, and they must not disagree:

- `Report` (``_validation_common.py``) assigns numbers, resolves
  ``R.ref(key)`` at render, and refuses a typed number in a report;
- ``scripts/check_finding_citations.py`` (``make
  lint-finding-citations``) refuses a typed number everywhere a report
  does not render, and resolves every key it cites (the word *finding*,
  then the key in backticks) against
  the keys some ``validate.py`` records.

An earlier draft gave each its own regex, and they disagreed on ``F32``
and ``F64``: one refused a dtype name, the other let it through. This
module imports nothing outside the standard library, so the lint, which
runs on the floor Python without the extension modules, loads it by path.

Examples
--------
>>> from doppler.tests._finding_keys import TYPED_F, cited_keys
>>> [m.group(0) for m in TYPED_F.finditer("F3, F12, CF32 and F64; F1-F2")]
['F3', 'F12', 'F1', 'F2']
>>> cited_keys(
...     "findings `pad_zero_was_one`, `unity_window` and "
...     "`code_dominates_loss`; the finding `unity_window` holds"
... )
['pad_zero_was_one', 'unity_window', 'code_dominates_loss', 'unity_window']
>>> cited_keys("/* the finding\\n * `wrapped_key` */ `not_cited`")
['wrapped_key']
"""

from __future__ import annotations

import ast
import re

#: A finding key: snake_case, starting with a letter, saying what the
#: finding is (``pfa_over_delivered``). A key is read in prose and in a C
#: comment, so it is bounded: past 32 characters it stops being a name.
KEY = re.compile(r"[a-z][a-z0-9_]*")

#: The longest key `Report.find` accepts.
KEY_MAX = 32

#: What `Report.ref(key)` returns, and `Report.render()` replaces with the
#: number. The brackets cannot occur in a report's own text, so the token
#: is unambiguous; it never survives a render.
REF = "[[F:{}]]"

#: A `REF` token, capturing its key.
REF_RE = re.compile(r"\[\[F:([^\]]*)\]\]")

#: A finding number written as text rather than derived from a key.
#:
#: ``F1`` to ``F99`` as a whole word, except ``F32`` and ``F64``, which are
#: dtype names (``CF32`` is not a whole-word ``F``). No report has reached
#: 32 findings; the most is RateSync's 17.
TYPED_F = re.compile(r"\bF(?!32\b|64\b)[1-9]\d?\b")

#: What may sit between the words of a citation: whitespace, bold markup,
#: a colon, or the leader of a comment the citation wraps inside (`` * ``,
#: ``#``, ``#:``, ``//``).
_GAP = r"[\s*#/:]"

#: How a page, a C comment or a test cites a finding: the word ``finding``
#: (or ``findings``), then one or more backticked keys joined by commas,
#: ``and`` or ``or``. Only this form is resolved, so it is the form to
#: write; a backticked identifier anywhere else is code, not a citation.
CITE = re.compile(
    rf"\b[Ff]indings?{_GAP}+"
    rf"(`[a-z][a-z0-9_]*`(?:{_GAP}*(?:,{_GAP}*(?:(?:and|or){_GAP}+)?"
    rf"|(?:and|or){_GAP}+)`[a-z][a-z0-9_]*`)*)"
)

#: One backticked key inside a `CITE` match.
_TICKED = re.compile(r"`([a-z][a-z0-9_]*)`")


def cited_keys(text: str) -> list[str]:
    """Every key `text` cites as a finding (see `CITE`), in order.

    Parameters
    ----------
    text : str
        Any text: a page, a comment, a docstring.

    Returns
    -------
    list of str
        The cited keys, repeats kept, in the order they appear.

    Examples
    --------
    >>> cited_keys("see finding **`unity_window`** and the `z` field")
    ['unity_window']
    """
    return [
        k for m in CITE.finditer(text) for k in _TICKED.findall(m.group(1))
    ]


def recorded_keys(source: str) -> list[str]:
    """The keys a ``validate.py`` records, read from its source.

    Every ``<report>.find("key", verdict, text)`` whose key is a string
    literal. Read from the source rather than by running the validator,
    because the lint must not depend on a build: a validator needs the
    extension modules, and some need a C harness besides.

    Parameters
    ----------
    source : str
        A ``validate.py``'s text.

    Returns
    -------
    list of str
        The literal keys, in source order. A key recorded in two exclusive
        branches appears twice.

    Examples
    --------
    >>> recorded_keys('R.find("pad_zero", "FIXED", "x")\\nfind("no", 1)')
    ['pad_zero']
    """
    keys = []
    for node in ast.walk(ast.parse(source)):
        if (
            isinstance(node, ast.Call)
            and isinstance(node.func, ast.Attribute)
            and node.func.attr == "find"
            and len(node.args) == 3
            and isinstance(node.args[0], ast.Constant)
            and isinstance(node.args[0].value, str)
            and KEY.fullmatch(node.args[0].value)
        ):
            keys.append((node.lineno, node.args[0].value))
    return [k for _, k in sorted(keys)]


def referenced_keys(source: str) -> list[tuple[int, str]]:
    """The literal keys a ``validate.py`` cites through ``ref``/``number``.

    ``R.ref(key)`` is resolved at render and ``R.number(key)`` when a
    figure is drawn, and only a write run draws figures, so a key passed
    to ``number`` that no ``find()`` records would pass every CI run and
    fail only the next time someone regenerates the plots. The lint
    therefore checks each one against the same file's `recorded_keys`.

    Parameters
    ----------
    source : str
        A ``validate.py``'s text.

    Returns
    -------
    list of (int, str)
        ``(line, key)`` for each call whose first argument is a string
        literal, in source order.

    Examples
    --------
    >>> referenced_keys('R.ref("a_key")\nR.number("b_key")\nR.ref(x)')
    [(1, 'a_key'), (2, 'b_key')]
    """
    out = []
    for node in ast.walk(ast.parse(source)):
        if (
            isinstance(node, ast.Call)
            and isinstance(node.func, ast.Attribute)
            and node.func.attr in ("ref", "number")
            and node.args
            and isinstance(node.args[0], ast.Constant)
            and isinstance(node.args[0].value, str)
        ):
            out.append((node.lineno, node.args[0].value))
    return sorted(out)
