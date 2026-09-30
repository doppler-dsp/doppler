#!/usr/bin/env python3
"""A refusal names its cause through ``const char **why``, and nothing else.

Every public function that can refuse its input and say why takes the reason
the same way: ``const char **why``. On refusal the function stores a pointer
to a **static** sentence there; on success it leaves it alone; the caller may
pass ``NULL``; the caller never frees it. ``dp_field_bits``, the three
``dp_wfm_*_from_json`` entry points and the frame parsers all agree.

That agreement was a habit and it was declared nowhere, so the next function
invented a second shape (``char *why, size_t why_cap`` -- a caller-owned
buffer the refusal formats into). A second shape is not a style difference:
jm's ``why = true`` and ``from_json_why`` bind exactly the first one, and a
buffer means an allocation on the error path and a formatted number where an
accessor belongs. The rule lives in
``docs/dev/contributing/error-convention.md``; this is its gate.

Derived, never registered: every ``*.h`` under ``native/inc/`` is scanned,
and every function parameter named ``why`` or ending ``_why`` must be
spelled exactly ``const char **`` (whitespace aside). A new header is
covered the moment it exists, and there is no list to forget to extend.

What it deliberately does NOT do: look inside a function-pointer
parameter's own parameter list, or judge a struct member. Neither exists in
this tree today; a gate that guesses at them would cost more than it finds.

Usage
-----
    python scripts/check_why_param.py    # exit 1 on any other spelling
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: The one spelling. Compared after `_normalise`, so `const char** why` and
#: `const char * *why` are the same declaration and both pass.
WANT = "const char**"

#: A C keyword that can precede a parenthesis; never a declared function.
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

#: `name (params)` closed by `;` (a declaration) or `{` (a header-inline
#: definition) -- the same declarator shape check_header_example_arity.py
#: reads, so the two gates agree on what a declaration is.
_DECL = re.compile(
    r"(?<![\w.>-])(\w+)\s*\(([^;{]*?)\)\s*(?:;|\{)",
    re.DOTALL,
)

#: A parameter's name: the last identifier, before any `[...]` suffix.
_PARAM_NAME = re.compile(r"(\w+)\s*(?:\[[^\]]*\]\s*)*$")


def _strip_comments_and_directives(src: str) -> str:
    """Blank comments and preprocessor lines, keeping newlines for line nos.

    A comment is where an example (``&why``) and prose about the rule live;
    a directive is where a macro would otherwise read as a declaration.
    """

    def blank(m: re.Match[str]) -> str:
        return re.sub(r"[^\n]", " ", m.group(0))

    src = re.sub(r"/\*.*?\*/", blank, src, flags=re.DOTALL)
    src = re.sub(r"//[^\n]*", blank, src)
    # A directive may continue across `\`-newlines; blank the whole run.
    return re.sub(r"^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", blank, src, flags=re.M)


def _split_top_level(text: str) -> list[str]:
    """Split on commas outside parens/brackets (function-pointer params)."""
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


def _normalise(type_: str) -> str:
    """Collapse whitespace, and drop it around ``*`` entirely."""
    t = re.sub(r"\s+", " ", type_).strip()
    return re.sub(r"\s*\*\s*", "*", t)


def why_params(src: str) -> list[tuple[int, str, str, str]]:
    """Every ``why`` parameter: ``(line, function, param, type)``."""
    code = _strip_comments_and_directives(src)
    found: list[tuple[int, str, str, str]] = []
    for m in _DECL.finditer(code):
        fn = m.group(1)
        if fn in _NOT_A_NAME:
            continue
        for p in _split_top_level(m.group(2)):
            p = p.strip()
            if "(" in p:  # a function-pointer parameter; see the docstring
                continue
            nm = _PARAM_NAME.search(p)
            if not nm:
                continue
            name = nm.group(1)
            if name != "why" and not name.endswith("_why"):
                continue
            type_ = p[: nm.start()]
            line = code[: m.start()].count("\n") + 1
            found.append((line, fn, name, _normalise(type_)))
    return found


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    # A gate that can only run against the real tree cannot be sabotaged.
    ap.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="repo root to scan (default: this checkout)",
    )
    args = ap.parse_args(argv[1:])
    root = args.root.resolve()
    headers = sorted((root / "native" / "inc").rglob("*.h"))

    seen = 0
    bad: list[str] = []
    for h in headers:
        rel = h.relative_to(root).as_posix()
        for line, fn, name, type_ in why_params(h.read_text(encoding="utf-8")):
            seen += 1
            if type_ != WANT:
                bad.append(
                    f"{rel}:{line}: {fn}() takes `{name}` as "
                    f"`{type_ or '(no type)'}`; a refusal reason is "
                    "`const char **`"
                )

    if bad:
        print("check_why_param: a refusal reason has a second shape — FAIL")
        for b in bad:
            print(f"  {b}")
        print(
            "\n  A refusal names its cause through `const char **why`: a\n"
            "  static sentence, written only on refusal, NULL allowed, never\n"
            "  freed. It is the one shape jm binds (why = true,\n"
            "  from_json_why)."
            " See docs/dev/contributing/error-convention.md."
        )
        return 1
    # The count is the proof the scan looked: zero on a tree that has them
    # would mean the parser went blind, not that the tree is clean.
    print(
        f"check_why_param: OK — {len(headers)} header(s), {seen} refusal-"
        f"reason parameter(s), every one `const char **`"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
