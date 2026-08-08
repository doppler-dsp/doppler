#!/usr/bin/env python3
"""Assert the two doc faces agree, section by section.

doppler documents a Python surface exactly once, in a C header's Doxygen
``@code`` block, and ``jm apply`` renders it onto **two** faces: the ``.pyi``
stub (what IDEs, mypy and the docs site read) and the runtime ``__doc__``
baked into the extension (what ``help()`` prints). The authoring guide states
that as an invariant — *one source, two faces* — but nothing enforced it.

It can silently break. A per-object ``native/src/<mod>/<mod>_ext_<obj>.c``
fragment is **sacred**: ``jm apply`` creates it once and thereafter only adds
*missing* members, so it will transplant a jm-derived built-in (``reset``,
``step``, ``steps``) but never rewrite an authored method's docstring. Edit a
header's ``@code`` for such a method and the stub moves while the fragment
does not — ``help()`` then shows an example the stub disagrees with, and no
existing gate notices: ``lint-stubs`` reads only the ``.pyi``, and the
coverage meter scores whether a runtime docstring *exists*, never whether it
still matches. That is exactly what the 71-column ``@code`` sweep hit: 45
divergent lines across 7 fragments, found by hand rather than by CI.

This checker closes that hole. For every method in every sacred fragment it
extracts each compared section from the C string literal, finds the same
method in the module's ``.pyi``, and compares.

Two sections are compared, and they fail for opposite reasons — the message
says which, because the fix differs:

``Examples``
    Authored once in a header ``@code``. A mismatch means the header was
    edited and only the stub followed; the fragment is hand-owned, so the fix
    is to update its string literal and clang-format it.

``Raises``
    Rendered by jm from the manifest's ``error``/``error_message`` — or, with
    ``exit`` (gh-805 §H), the finalizer's inherited pair. **Both** faces are
    generated, so a difference is a codegen bug and hand-patching the two into
    agreement would only hide it. Report it instead. This is not theoretical:
    a teardown inheriting its finalizer's error reached the raise and the
    runtime ``__doc__`` as ``ValueError`` while the stub said ``RuntimeError``,
    and nothing noticed until it was read by hand.

Comparison is on the *content* of each section, whitespace-normalised
per line, because the two faces legitimately differ in framing: the stub
indents 8 columns inside a class body, while the C literal is flush and is
split across adjacent string tokens wherever clang-format needed to fit 79
columns. Neither of those is a divergence; a changed doctest is.

Exit status is 0 when every matched method agrees, 1 otherwise. Methods
present in a fragment but absent from the stub (aliases, view-only members)
are reported as unmatched and do not fail the run — they carry no stub face
to disagree with.

Examples
--------
Run it directly, or via ``make lint``:

    $ python scripts/check_doc_face_parity.py
    Doc face parity: OK — 214 methods compared, 0 divergent
"""

from __future__ import annotations

import argparse
import ast
import pathlib
import re
import sys

# The numpy sections compared across the two faces. Both are rendered by jm
# from ONE input onto two faces, so either can move on one face alone:
# `Examples` from a header's @code block, `Raises` from the manifest's
# `error`/`error_message` (or, with `exit`, the finalizer's inherited pair).
SECTIONS = ("Examples", "Raises")

REPO = pathlib.Path(__file__).resolve().parent.parent
NATIVE_SRC = REPO / "native" / "src"
PYEXT_DIR = REPO / "src" / "doppler"

# A PyMethodDef row opens with the Python-visible name, then the C function.
# The docstring is the run of adjacent string literals before the closing
# brace. Non-greedy up to `},` keeps one row from swallowing the next.
_METHOD_ROW = re.compile(
    r'\{\s*"(?P<name>\w+)"\s*,\s*\(PyCFunction\)'
    r"(?P<body>.*?)"
    r"\}\s*,",
    re.DOTALL,
)
# One C string literal, escapes tolerated.
_C_STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
# .tp_name = "<module>.<Class>" gives the fragment's Python class.
_TP_NAME = re.compile(r'\.tp_name\s*=\s*"(?:[\w.]*\.)?(?P<cls>\w+)"')


def c_literal_text(body: str) -> str:
    """Join a run of adjacent C string literals into the string they form.

    clang-format splits a long literal across tokens to fit 79 columns, so
    the docstring a reader sees at runtime is the *concatenation*, not any
    single token. Unescapes ``\\n`` and ``\\"`` so the result is the real
    text; other escapes are left alone (docstrings use no others here).
    """
    joined = "".join(_C_STRING.findall(body))
    return joined.replace('\\"', '"').replace("\\n", "\n")


def section_block(doc: str, section: str) -> list[str] | None:
    """Return the numpy *section* body of *doc*, or None if it has none.

    The block runs from the ``--------`` underline to the end of the
    docstring or to the next numpy section header (a line whose successor is
    a run of dashes). Lines are stripped so an indent difference between the
    two faces is not read as a divergence.

    Section-agnostic because the divergence it catches is not: `Examples`
    comes from a header's ``@code`` and `Raises` from the manifest, but both
    are rendered by jm onto two faces from one input, and either can move on
    one face alone.
    """
    lines = doc.split("\n")
    for i, ln in enumerate(lines):
        if ln.strip() != section:
            continue
        if i + 1 >= len(lines) or set(lines[i + 1].strip()) != {"-"}:
            continue
        out: list[str] = []
        for j in range(i + 2, len(lines)):
            nxt = lines[j + 1].strip() if j + 1 < len(lines) else ""
            if lines[j].strip() and nxt and set(nxt) == {"-"}:
                break  # next section header
            out.append(lines[j].strip())
        while out and not out[-1]:
            out.pop()
        return out
    return None


def fragment_methods(path: pathlib.Path) -> tuple[str | None, dict]:
    """``(class_name, {(method, section): body})`` for one sacred fragment."""
    text = path.read_text()
    m = _TP_NAME.search(text)
    cls = m.group("cls") if m else None
    found = {}
    for row in _METHOD_ROW.finditer(text):
        doc = c_literal_text(row.group("body"))
        for section in SECTIONS:
            body = section_block(doc, section)
            if body is not None:
                found[(row.group("name"), section)] = body
    return cls, found


def stub_methods(path: pathlib.Path) -> dict:
    """``{(class, method, section): body}`` for one ``.pyi``.

    Parsed with ``ast`` rather than by regex: a stub is valid Python, and the
    docstring is exactly ``ast.get_docstring`` of the function node.
    """
    tree = ast.parse(path.read_text())
    out = {}
    for node in tree.body:
        if not isinstance(node, ast.ClassDef):
            continue
        for member in node.body:
            if not isinstance(member, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            doc = ast.get_docstring(member, clean=False)
            if not doc:
                continue
            for section in SECTIONS:
                body = section_block(doc, section)
                if body is not None:
                    out[(node.name, member.name, section)] = body
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Check doc face parity.")
    ap.add_argument(
        "--verbose",
        action="store_true",
        help="list every method compared, not just the divergent ones",
    )
    args = ap.parse_args()

    stubs: dict = {}
    for pyi in sorted(PYEXT_DIR.rglob("*.pyi")):
        stubs.update(stub_methods(pyi))

    compared = 0
    unmatched: list[str] = []
    bad: list[tuple[str, str, str, list[str], list[str]]] = []

    for frag in sorted(NATIVE_SRC.rglob("*_ext_*.c")):
        cls, methods = fragment_methods(frag)
        if cls is None:
            continue
        rel = frag.relative_to(REPO)
        for (name, section), runtime_ex in sorted(methods.items()):
            stub_ex = stubs.get((cls, name, section))
            if stub_ex is None:
                unmatched.append(f"{rel}: {cls}.{name} ({section})")
                continue
            compared += 1
            if stub_ex != runtime_ex:
                bad.append(
                    (str(rel), cls, f"{name} [{section}]", stub_ex, runtime_ex)
                )
            elif args.verbose:
                print(f"  ok  {cls}.{name} ({section})")

    for rel, cls, name, stub_ex, runtime_ex in bad:
        print(f"\n{rel}: {cls}.{name} — differs between faces")
        only_stub = [ln for ln in stub_ex if ln not in runtime_ex]
        only_rt = [ln for ln in runtime_ex if ln not in stub_ex]
        for ln in only_stub:
            print(f"    stub only:    {ln}")
        for ln in only_rt:
            print(f"    runtime only: {ln}")

    if args.verbose and unmatched:
        print("\nno stub counterpart (not a failure):")
        for u in unmatched:
            print(f"    {u}")

    if bad:
        print(
            f"\nDoc face parity: FAIL — {len(bad)} of {compared} methods "
            f"diverge between the .pyi and the runtime __doc__.\n"
            "One input, two faces, and only one of them moved.\n"
            "  Examples: the header's @code was edited and the sacred "
            "_ext_<obj>.c fragment did not follow — it is hand-owned, so "
            "update its string literal and clang-format it.\n"
            "  Raises: both faces are jm's, from the manifest, so a "
            "difference here is a CODEGEN bug, not something to hand-patch "
            "into agreement. Report it."
        )
        return 1

    print(
        f"Doc face parity: OK — {compared} methods compared, 0 divergent "
        f"({len(unmatched)} fragment methods have no stub counterpart)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
