"""Fail-closed gate: every C ``@code`` block in a header compiles and runs.

``adding-algorithms.md`` says a header's ``@code`` blocks are tests: jm
flows each into the ``.pyi`` and ``make test-stubs`` executes it. That holds
for a ``>>>`` block, which is the Python binding's example. A block written
in **C** reaches no ``.pyi`` and no stub doctest, so until this gate nothing
compiled it -- ``dp_wfm_field_render``'s example was copied by hand into a C
test, and a copy drifts from its source silently (#1635). The arity-only
half of the same hole is ``scripts/check_header_example_arity.py``
(doppler#1082); this is the compiler half.

**Which blocks.** Derived from the block itself, not registered and not
looked up: a block containing ``>>>`` is Python and belongs to
``make test-stubs``; a ``@code{.lang}`` block names its own language
(``{.unparsed}`` is a grammar, not a program) and is not C; every other
``@code`` block under ``native/inc/**`` is C and is gated here. Deriving
"has a binding" from the manifests instead would be a second copy of jm's
naming rules, and would still leave a C example on a *bound* function
executed by nothing -- the language the example is written in is what
decides which harness can run it.

**How.** Exactly the ``docs/`` C-fence recipe (``test_c_doc_snippets.py``'s
``_compile_and_run`` -- reused, not copied): ``-std=gnu99 -Wall -Wextra
-Werror`` against ``build/libdoppler.a``, then run, exit 0. A header
example is usually a fragment, so one with no ``main`` is wrapped: the
standard headers an example leans on, the header it lives in, and its body
inside ``int main (void) { ... return 0; }``. Unused-variable warnings are
off for a wrapped fragment only -- ``size_t n = f (...);  // 124`` is how an
example shows a return value, and it is not a defect.

**Escapes**, visible in the block and reviewed, reason mandatory, as the
first line of the block::

    // header-code: no-run=REASON   compile -Werror, never execute
    // header-code: skip=REASON     neither (an illustrative fragment)

**Backlog.** Blocks that did not compile or run when the gate landed are
listed in ``native/.c-header-code-ignore`` -- a RATCHET: an entry that now
passes, or no longer exists, fails ``test_ignore_list_not_stale`` until it
is deleted, so the list can only shrink (#1651).

Run with ``make test-snippets`` (after ``make build``).
"""

from __future__ import annotations

import re

import pytest

from doppler.tests._repo import repo_root
from doppler.tests.test_c_doc_snippets import _compile_and_run

REPO = repo_root(__file__)
INC = REPO / "native" / "inc"
IGNORE_FILE = REPO / "native" / ".c-header-code-ignore"

#: `@code` or `@code{.lang}`, through `@endcode`, inside a doxygen comment.
_BLOCK = re.compile(
    r"@code(\{[^}]*\})?[ \t]*\\?[ \t]*\n(.*?)@endcode", re.DOTALL
)
#: A doc comment inside a multi-line macro carries a `\` on every line.
_CONTINUATION = re.compile(r"[ \t]*\\$", re.MULTILINE)
#: The comment gutter doxygen strips: leading space, one `*`, one space.
_GUTTER = re.compile(r"^[ \t]*\* ?", re.MULTILINE)
#: The function a doc comment documents: the first `name (` after its
#: closing `*/`, or `(*name)(` for a callback typedef.
_DECL_NAME = re.compile(r"\b(\w+)\s*\)?\s*\(")
#: A word before `(` that is a type or keyword, never the name.
_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
_NOT_A_NAME = frozenset(
    {
        "void",
        "int",
        "char",
        "float",
        "double",
        "size_t",
        "return",
        "sizeof",
        "defined",
        "if",
        "while",
        "for",
        "switch",
    }
)
_MARKER = re.compile(r"^\s*//\s*header-code:\s*(\w[\w-]*)\s*=\s*(.*?)\s*$")
_MAIN = re.compile(r"\bint\s+main\s*\(")

#: Headers a fragment may lean on without including -- what every doppler
#: example assumes is in scope. Explicit includes in a block still work.
_PRELUDE = """\
#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
"""

#: Only for a wrapped fragment: an example names a result to show it.
_FRAGMENT_CFLAGS = ("-Wno-unused-variable", "-Wno-unused-but-set-variable")


def _documented(text, pos):
    """The name declared after the doc comment that holds offset *pos*."""
    end = text.find("*/", pos)
    if end < 0:
        return "file"
    # Comments out, or the prose of the NEXT doc comment ("the noise_est
    # (a running mean)") would be read as a declaration.
    rest = _COMMENT.sub(" ", text[end + 2 :])
    for d in _DECL_NAME.finditer(rest):
        if d.group(1) not in _NOT_A_NAME:
            return d.group(1)
    return "file"


def _blocks():
    """``[(block id, header rel path, C source)]`` for every C block.

    The id is ``<header>::<documented function>``, with ``#n`` appended from
    the second block on the same function -- stable across edits elsewhere
    in the header, unlike an ordinal.
    """
    out = []
    for h in sorted(INC.rglob("*.h")):
        text = h.read_text(encoding="utf-8")
        rel = h.relative_to(INC).as_posix()
        seen: dict[str, int] = {}
        for m in _BLOCK.finditer(text):
            lang, raw = m.group(1), m.group(2)
            if lang or ">>>" in raw:
                continue
            code = _GUTTER.sub("", _CONTINUATION.sub("", raw))
            code = code.rstrip() + "\n"
            name = _documented(text, m.end())
            k = seen.get(name, 0)
            seen[name] = k + 1
            out.append((f"{rel}::{name}" + (f"#{k}" if k else ""), rel, code))
    return out


def _load_ignore():
    if not IGNORE_FILE.exists():
        return set()
    out = set()
    for line in IGNORE_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            out.add(line)
    return out


BLOCKS = _blocks()
IGNORED = _load_ignore()
CASES = [b for b in BLOCKS if b[0] not in IGNORED]


def _translation_unit(rel, code):
    """``(source, extra cflags)``: the block as a program ``cc`` can build."""
    if _MAIN.search(code):
        return f'{_PRELUDE}#include "{rel}"\n\n{code}', ()
    body = "\n".join("  " + ln if ln else ln for ln in code.splitlines())
    src = (
        f'{_PRELUDE}#include "{rel}"\n\nint\nmain (void)\n{{\n'
        f"{body}\n  return 0;\n}}\n"
    )
    return src, _FRAGMENT_CFLAGS


def _check(blockid, rel, code, tmp_path):
    first = code.lstrip().splitlines()[0] if code.strip() else ""
    run = True
    m = _MARKER.match(first)
    if m:
        kind, reason = m.group(1), m.group(2)
        assert reason, f"{blockid}: header-code {kind}= needs a reason"
        if kind == "skip":
            return
        assert kind == "no-run", (
            f"{blockid}: unknown header-code marker {kind!r} "
            "(expected no-run= or skip=)"
        )
        run = False
    src, extra = _translation_unit(rel, code)
    _compile_and_run(blockid, src, tmp_path, run=run, extra_cflags=extra)


@pytest.mark.docs_snippets
@pytest.mark.parametrize("blockid,rel,code", CASES, ids=[c[0] for c in CASES])
def test_c_header_code(blockid, rel, code, tmp_path):
    """Compile + run one header ``@code`` block; fail naming it."""
    _check(blockid, rel, code, tmp_path)


@pytest.mark.docs_snippets
def test_discovery_nonempty():
    assert BLOCKS, "no C @code blocks under native/inc -- parser broken?"


@pytest.mark.docs_snippets
def test_ignore_list_not_stale(tmp_path):
    """The backlog is a ratchet: every entry must still exist AND fail.

    An entry that names no block (renamed, deleted) or whose block now
    passes fails here, so a fix has to take its line with it.
    """
    by_id = {b[0]: b for b in BLOCKS}
    gone = sorted(IGNORED - set(by_id))
    assert not gone, (
        f"{IGNORE_FILE.name} names blocks that no longer exist -- "
        f"remove them: {gone}"
    )
    fixed = []
    for i, bid in enumerate(sorted(IGNORED)):
        d = tmp_path / str(i)
        d.mkdir()
        try:
            _check(*by_id[bid], d)
        except AssertionError:
            continue
        fixed.append(bid)
    assert not fixed, (
        f"these blocks now pass -- delete them from {IGNORE_FILE.name}: "
        f"{fixed}"
    )


@pytest.mark.docs_snippets
def test_wrapping_catches_a_renamed_function(tmp_path):
    """Sabotage, pinned: a block calling a function that does not exist.

    Proves the wrapper does not swallow an implicit declaration. A
    docs_snippets case like the rest, because it links ``libdoppler.a``:
    the plain Python suite runs where that archive is not built (Windows'
    clang-cl job has none), and a missing archive is not this test's
    subject.
    """
    code = "size_t n = dp_no_such_function (1);\n"
    with pytest.raises(AssertionError, match="failed to compile"):
        _check("sabotage::renamed", "doppler/dp_state.h", code, tmp_path)
