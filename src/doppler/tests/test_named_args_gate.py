"""The named-arguments gate, exercised over a seeded tree.

`scripts/check_named_args.py` holds the rule that an integer-coded argument
is passed by name: a PSD window or averaging mode, or a Spectrogram mode, is
never an integer literal. The constructors take a plain `int`, so the
compiler cannot refuse one. It began as the owner's rule on #1968 (dB rows
are asked for by name; #2097 renumbered the modes, so a call written as a
number would have changed meaning without a diff), and #1894's review found
283 more, one hidden behind a comment.

The cases seed a small git repository, because the gate reads `git
ls-files` and a gate that can only run against the real tree cannot be
sabotaged without breaking doppler. The seed carries the C enums the gate
reads its names from, and one call of every function in its table, since
each row must find a call. They cover the shapes the tree actually has: a
call on one line, one wrapped inside a header's `@code` block with `*`
continuations, a Markdown fence, casts, parentheses and comments, a prose
mention and a string literal that are not calls, the two skipped trees, and
`--fix`, which must keep the whitespace in front of what it rewrites.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import header_root, repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_named_args.py"


def _gate():  # type: ignore[no-untyped-def]
    """The gate as a module, for its tables and its proof helper."""
    sys.path.insert(0, str(SCRIPT.parent))
    try:
        import check_named_args as g
    finally:
        sys.path.remove(str(SCRIPT.parent))
    return g


#: The name a seeded call passes in each enum's slot.
_NAME = {
    "psd_window": "DP_PSD_WINDOW_HANN",
    "spectrogram_mode": "DP_SPECTROGRAM_POWER",
}


def _helper_src(rows: dict, literal: tuple[str, int] | None = None) -> str:
    """One file's seeded helpers: each one's definition (every argument a
    declaration, so not a call) and one call passing its slots by name -- or,
    for `literal` = (function, slot), that one slot as the number 1."""
    defs, calls = [], []
    for func, (nargs, slots) in rows.items():
        params = ", ".join(f"int a{i}" for i in range(nargs))
        defs.append(f"static int {func} ({params});")
        args = []
        for i in range(nargs):
            if literal == (func, i):
                args.append("1")
            elif i in slots:
                args.append(_NAME[slots[i][1]])
            else:
                args.append(f"x{i}")
        calls.append(f"  {func} ({', '.join(args)});")
    return "\n".join([*defs, "void seeded (void)", "{", *calls, "}", ""])


def _helper_files(
    literal: tuple[str, str, int] | None = None,
) -> dict[str, str]:
    """Every HELPERS file, seeded; `literal` = (path, function, slot)."""
    return {
        path: _helper_src(
            rows, literal[1:] if literal and literal[0] == path else None
        )
        for path, rows in _gate().HELPERS.items()
    }


#: Where the gate reads the bound C enums, relative to the root it is given.
_HDR = header_root().relative_to(repo_root())

#: The enums the gate names literals against, annotated as the real headers
#: are (check_wfm_enum_tables.py's rule 7 holds those to the manifest).
_ENUMS_H = """\
/* SSOT: enum=psd_window */
enum {
    DP_PSD_WINDOW_HANN = 0, DP_PSD_WINDOW_KAISER = 1,
    DP_PSD_WINDOW_BLACKMAN_HARRIS = 2, DP_PSD_WINDOW_RECT = 3
};
/* SSOT: enum=spectrogram_mode */
enum { DP_SPECTROGRAM_POWER = 0, DP_SPECTROGRAM_DB = 1 };
/* SSOT: enum=acc_trace_mode */
typedef enum {
    ACC_TRACE_MEAN = 0, ACC_TRACE_EXP = 1, ACC_TRACE_MAXHOLD = 2,
    ACC_TRACE_MINHOLD = 3
} acc_trace_mode_t;
"""

#: One call of every table row, each slot passed in a shape that must pass.
_NAMED_C = """\
#include "doppler/spectrogram/spectrogram_core.h"
int main (void)
{
  int mode = DP_SPECTROGRAM_DB;
  dp_spectrogram_state_t *a = dp_spectrogram_create (
      8, 4, DP_PSD_WINDOW_HANN, 0.0f, DP_SPECTROGRAM_POWER);
  dp_spectrogram_state_t *b = dp_spectrogram_create (8, 4, (int)w[1],
                                                     0.0f, mode);
  dp_psd_state_t *p = dp_psd_create (64, 1.0, DP_PSD_WINDOW_KAISER, 8.0f, 1,
                                     1.0, 0, ACC_TRACE_MEAN, 0.1);
  dp_psd_state_t *q = dp_psd_create_hop (64, 1.0, win, 0.0f, 1, 1.0, 0,
                                         ACC_TRACE_EXP, 0.5, 16);
  dp_carrier_acq_state_t *c = dp_carrier_acq_create (
      4e6, 1e5, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL, 0, 1e-3, 0.9, 2.0,
      true, 1000);
  dp_acc_trace_state_t *t = dp_acc_trace_create (8, ACC_TRACE_MEAN, 0.1);
  dp_psd_kernel_t k;
  int ok = dp_psd_kernel_init (&k, 64, DP_PSD_WINDOW_HANN, 0.0f, 1, 1.0);
  printf ("dp_psd_create(%d, 0, 0, 0, 0, 0, 0, 0, 0) is no call\\n", 9);
  return !a || !b || !p || !q || !c || !t || ok;
}
"""

_NAMED_H = """\
/**
 * @code
 * dp_spectrogram_state_t *s = dp_spectrogram_create (8, 4, 3, 0.0f,
 *                                                    DP_SPECTROGRAM_DB);
 * @endcode
 */
dp_spectrogram_state_t *dp_spectrogram_create (size_t nfft, size_t hop,
                                               int window, float beta,
                                               int mode);
"""
# The @code call's window is a literal on purpose in ONE case below; the
# baseline names it.
_NAMED_H_OK = _NAMED_H.replace("(8, 4, 3,", "(8, 4, DP_PSD_WINDOW_RECT,")

_NAMED_MD = """\
# A page

Call `dp_spectrogram_create()` to make one.

```c
dp_spectrogram_state_t *s = dp_spectrogram_create (
    1024, 256, DP_PSD_WINDOW_HANN, 0.0f, DP_SPECTROGRAM_POWER);
```
"""


def _seed(tmp_path: Path, **override: str) -> None:
    files = {
        f"{_HDR}/psd/psd_core.h": _ENUMS_H,
        "a.c": _NAMED_C,
        "inc/s.h": _NAMED_H_OK,
        "docs/p.md": _NAMED_MD,
        **_helper_files(),
    }
    files.update(override)
    for rel, text in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    subprocess.run(["git", "-C", str(tmp_path), "add", "."], check=True)


def _run(tmp_path: Path, *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path), *extra],
        capture_output=True,
        text=True,
        check=False,
    )


def test_named_arguments_pass_in_every_shape(tmp_path: Path) -> None:
    """The baseline: without it every RED below proves nothing."""
    _seed(tmp_path)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    # seven calls in a.c (the printf is a string), the @code call in s.h (its
    # declaration is not a call), the fence in p.md; the prose mention is
    # not a call either. One call of each HELPERS row, in its two files.
    helpers = _gate().HELPERS
    n = sum(len(rows) for rows in helpers.values())
    files = 3 + len(helpers)
    assert f"9 call(s) and {n} helper call(s) in {files} file(s)" in r.stdout


def test_a_literal_mode_fails_and_names_the_line(tmp_path: Path) -> None:
    bad = _NAMED_C.replace("0.0f, DP_SPECTROGRAM_POWER);", "0.0f, 1);")
    _seed(tmp_path, **{"a.c": bad})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert (
        "a.c:5: dp_spectrogram_create mode = 1 (write DP_SPECTROGRAM_DB)"
        in r.stdout
    )


@pytest.mark.parametrize(
    "spelling", ["0", "(1)", "(int) 0", "0x1", "1u", "-1", "'\\0'", "010"]
)
def test_every_spelling_of_a_literal_fails(
    tmp_path: Path, spelling: str
) -> None:
    bad = _NAMED_C.replace(
        "0.0f, DP_SPECTROGRAM_POWER);", f"0.0f, {spelling});"
    )
    _seed(tmp_path, **{"a.c": bad})
    r = _run(tmp_path)
    assert r.returncode == 1, (spelling, r.stdout)
    assert "a.c:5: dp_spectrogram_create mode" in r.stdout


def test_a_literal_behind_a_comment_fails(tmp_path: Path) -> None:
    """The shape the old gate missed in four library files: `1 /* Kaiser */`
    reads as a name to a human and was not a bare literal to the parser."""
    bad = _NAMED_C.replace(
        "DP_PSD_WINDOW_KAISER, 8.0f", "1 /* Kaiser */, 8.0f"
    )
    _seed(tmp_path, **{"a.c": bad})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert (
        "a.c:9: dp_psd_create window = 1 (write DP_PSD_WINDOW_KAISER)"
        in r.stdout
    )


@pytest.mark.parametrize(
    ("named", "literal", "where"),
    [
        (
            " 8, 4, DP_PSD_WINDOW_HANN,",
            " 8, 4, 2,",
            "a.c:5: dp_spectrogram_create window = 2"
            " (write DP_PSD_WINDOW_BLACKMAN_HARRIS)",
        ),
        (
            "0, ACC_TRACE_MEAN, 0.1",
            "0, 3, 0.1",
            "a.c:9: dp_psd_create mode = 3 (write ACC_TRACE_MINHOLD)",
        ),
        (
            "(8, ACC_TRACE_MEAN, 0.1)",
            "(8, 2, 0.1)",
            "a.c:16: dp_acc_trace_create mode = 2 (write ACC_TRACE_MAXHOLD)",
        ),
        (
            "(64, 1.0, win,",
            "(64, 1.0, 3,",
            "a.c:11: dp_psd_create_hop window = 3 (write DP_PSD_WINDOW_RECT)",
        ),
        (
            "ACC_TRACE_EXP, 0.5",
            "1, 0.5",
            "a.c:11: dp_psd_create_hop mode = 1 (write ACC_TRACE_EXP)",
        ),
        (
            "(&k, 64, DP_PSD_WINDOW_HANN,",
            "(&k, 64, 3,",
            "a.c:18: dp_psd_kernel_init window = 3 (write DP_PSD_WINDOW_RECT)",
        ),
        (
            "0.0, 4, DP_PSD_WINDOW_HANN,",
            "0.0, 4, 0,",
            "a.c:13: dp_carrier_acq_create window = 0"
            " (write DP_PSD_WINDOW_HANN)",
        ),
    ],
)
def test_every_table_slot_is_checked(
    tmp_path: Path, named: str, literal: str, where: str
) -> None:
    """One case per (function, slot) in TABLE, so a row that stopped being
    read cannot hide behind the others."""
    assert _NAMED_C.count(named) == 1
    _seed(tmp_path, **{"a.c": _NAMED_C.replace(named, literal)})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert where in r.stdout


def test_a_literal_wrapped_in_a_header_comment_fails(tmp_path: Path) -> None:
    _seed(tmp_path, **{"inc/s.h": _NAMED_H})
    r = _run(tmp_path)
    assert r.returncode == 1
    # git's path, `/` on every OS, and ASCII: Windows printed `inc\\s.h`
    # and a cp1252 replacement for an em-dash (#2097's clang-cl leg)
    assert "inc/s.h:3: dp_spectrogram_create window = 3" in r.stdout
    assert r.stdout.isascii()


def test_a_literal_in_a_markdown_fence_fails(tmp_path: Path) -> None:
    bad = _NAMED_MD.replace("DP_SPECTROGRAM_POWER", "0")
    _seed(tmp_path, **{"docs/p.md": bad})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "docs/p.md:6: dp_spectrogram_create mode = 0" in r.stdout


def test_an_untracked_file_is_not_the_gates(tmp_path: Path) -> None:
    _seed(tmp_path)
    (tmp_path / "scratch.c").write_text(
        "x = dp_spectrogram_create (8, 4, 0, 0.0f, 0);\n", encoding="utf-8"
    )
    assert _run(tmp_path).returncode == 0


def test_generated_c_api_pages_are_not_the_gates(tmp_path: Path) -> None:
    """docs/c-api is generated from the headers already scanned."""
    _seed(
        tmp_path,
        **{
            "docs/c-api/x.md": (
                "x = dp_spectrogram_create (8, 4, 0, 0.0f, 0);\n"
            )
        },
    )
    assert _run(tmp_path).returncode == 0


def test_example_projects_are_exempt_for_the_window_slot_only(
    tmp_path: Path,
) -> None:
    """The example projects build against a released doppler, which has the
    mode names but no DP_PSD_WINDOW_* yet (#2186): their window may be a
    number, and every other slot is held."""
    ok = "x = dp_spectrogram_create (8, 4, 0, 0.0f, DP_SPECTROGRAM_POWER);\n"
    _seed(tmp_path, **{"example-projects/q/m.c": ok})
    assert _run(tmp_path).returncode == 0
    bad = "x = dp_psd_create (8, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);\n"
    _seed(tmp_path / "bad", **{"example-projects/q/m.c": bad})
    r = _run(tmp_path / "bad")
    assert r.returncode == 1
    assert "example-projects/q/m.c:1: dp_psd_create mode = 0" in r.stdout
    assert "window" not in r.stdout.split("FAIL", 1)[1].split("Pass")[0]


@pytest.mark.parametrize(
    ("arg", "lit"),
    [
        ("1 /* Kaiser, beta 8 */", "1"),
        ("1 // Kaiser, then a stray )\n", "1"),
        ("(unsigned int)1", "1"),
        ("(uint32_t) 1", "1"),
    ],
)
def test_a_literal_hidden_by_a_comma_comment_or_cast_fails(
    tmp_path: Path, arg: str, lit: str
) -> None:
    """A comma or `)` inside a comment split the call and hid it entirely;
    a two-word or typedef'd cast was not seen as a cast. Each is a literal
    in the window slot."""
    bad = _NAMED_C.replace("DP_PSD_WINDOW_KAISER, 8.0f", f"{arg}, 8.0f")
    _seed(tmp_path, **{"a.c": bad})
    r = _run(tmp_path)
    assert r.returncode == 1, r.stdout
    assert f"a.c:9: dp_psd_create window = {lit}" in r.stdout
    assert "OK --" not in r.stdout  # the gate did not pass it


@pytest.mark.parametrize("expr", ["(base) + 1", "(n) - 1", "w + 0"])
def test_an_expression_is_not_a_literal(tmp_path: Path, expr: str) -> None:
    """`(base) + 1` is a sum, not a cast of `+ 1`; reading it as one made
    --fix write `(base) DP_PSD_WINDOW_KAISER`, which does not compile."""
    src = _NAMED_C.replace("DP_PSD_WINDOW_KAISER, 8.0f", f"{expr}, 8.0f")
    _seed(tmp_path, **{"a.c": src})
    r = _run(tmp_path, "--fix")
    assert r.returncode == 0, r.stdout
    assert (tmp_path / "a.c").read_text(encoding="utf-8") == src


def test_a_call_with_the_wrong_argument_count_fails(tmp_path: Path) -> None:
    """A signature change must not leave a stale row checking nothing: a call
    whose count is not the row's is a finding, not a skip."""
    src = _NAMED_C.replace(
        "  dp_acc_trace_state_t *t",
        "  dp_psd_create (64, 1.0, DP_PSD_WINDOW_HANN);\n"
        "  dp_acc_trace_state_t *t",
    )
    _seed(tmp_path, **{"a.c": src})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert (
        "a.c:16: dp_psd_create called with 3 argument(s), its row says 9"
        in r.stdout
    )


def test_an_ellipsis_mention_is_prose(tmp_path: Path) -> None:
    """`dp_psd_create(1 << 62, ...)` in a report names a call, not one."""
    _seed(tmp_path, **{"docs/q.md": "`dp_psd_create(1 << 62, ...)` crashed\n"})
    assert _run(tmp_path).returncode == 0


def test_fix_proves_the_rewrite_before_making_it() -> None:
    """--fix rewrites a token only if the argument then reduces to exactly
    the name; anything else is reported and left alone."""
    g = _gate()
    assert g._fix_span("(int) 1", 0, 7, "K") == (6, 7)
    assert g._fix_span("1 /* 2, 3 */", 0, 12, "K") == (0, 1)
    assert g._fix_span("(base) + 1", 0, 10, "K") is None
    assert g._fix_span("w + 1", 0, 5, "K") is None


def test_a_row_whose_function_is_never_called_fails(tmp_path: Path) -> None:
    """Fail-closed per row: a renamed function would otherwise check nothing
    and read as clean, however many calls the other rows found."""
    no_acq = _NAMED_C.replace("dp_carrier_acq_create (", "acq_create (")
    _seed(tmp_path, **{"a.c": no_acq})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "found no dp_carrier_acq_create call" in r.stdout
    assert "scan is broken" in r.stdout


def test_a_slot_whose_enum_no_header_binds_fails(tmp_path: Path) -> None:
    """The names come from the bound C enums; without one there is nothing
    to check a slot against, which is a failure, not a pass."""
    _seed(
        tmp_path,
        **{f"{_HDR}/psd/psd_core.h": _ENUMS_H.replace("enum=psd_window", "")},
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "no C enum carries `SSOT: enum=psd_window`" in r.stdout


def test_fix_names_the_literals_and_keeps_the_layout(tmp_path: Path) -> None:
    """--fix rewrites each literal to its name in place. The whitespace in
    front of a literal is not the literal's: the first version took it, and
    turned `HOP, 0` into `HOP,DP_PSD_WINDOW_HANN` and joined a line that
    began with one."""
    bad = (
        _NAMED_C.replace(" 8, 4, DP_PSD_WINDOW_HANN,", " 8, 4, 0,")
        .replace("DP_PSD_WINDOW_KAISER, 8.0f", "1 /* Kaiser */, 8.0f")
        .replace(
            "                                     1.0, 0, ACC_TRACE_MEAN,",
            "                                     1.0, 0,\n"
            "                                     0,",
        )
    )
    _seed(tmp_path, **{"a.c": bad})
    r = _run(tmp_path, "--fix")
    assert r.returncode == 0, r.stdout
    assert "named 3 literal(s)" in r.stdout
    fixed = (tmp_path / "a.c").read_text(encoding="utf-8")
    assert " 8, 4, DP_PSD_WINDOW_HANN, 0.0f," in fixed
    assert "DP_PSD_WINDOW_KAISER /* Kaiser */, 8.0f" in fixed
    assert (
        "1.0, 0,\n                                     ACC_TRACE_MEAN, 0.1"
        in fixed
    )
    assert _run(tmp_path).returncode == 0


def test_fix_leaves_a_value_no_enum_has(tmp_path: Path) -> None:
    """A literal that is no value of its enum has no name: --fix must not
    invent one, and the gate still refuses it."""
    bad = _NAMED_C.replace(" 8, 4, DP_PSD_WINDOW_HANN,", " 8, 4, 4,")
    _seed(tmp_path, **{"a.c": bad})
    r = _run(tmp_path, "--fix")
    assert r.returncode == 1
    assert "window = 4 (no such value)" in r.stdout
    assert (tmp_path / "a.c").read_text(encoding="utf-8") == bad


def test_the_real_tree_passes() -> None:
    """The gate against doppler itself, without --fix, so the seeded cases
    are not the only thing it has ever been pointed at."""
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout


def _helper_slots() -> list[tuple[str, str, int, str]]:
    return [
        (path, func, idx, param)
        for path, rows in _gate().HELPERS.items()
        for func, (_, slots) in rows.items()
        for idx, (param, _) in slots.items()
    ]


@pytest.mark.parametrize(("path", "func", "idx", "param"), _helper_slots())
def test_every_helper_slot_is_checked(
    tmp_path: Path, path: str, func: str, idx: int, param: str
) -> None:
    """One case per (file, helper, slot) in HELPERS (#2191): a literal that
    reaches a constructor through a helper is refused in that helper's
    file, as a direct call's is."""
    _seed(tmp_path, **_helper_files((path, func, idx)))
    r = _run(tmp_path)
    assert r.returncode == 1, r.stdout
    assert f"{path}:" in r.stdout
    assert f": {func} {param} = 1 (write " in r.stdout


def test_fix_names_a_helper_literal(tmp_path: Path) -> None:
    path = "native/validation/spectrogram_certify.c"
    _seed(tmp_path, **_helper_files((path, "make", 3)))
    r = _run(tmp_path, "--fix")
    assert r.returncode == 0, r.stdout
    assert "named 1 literal(s)" in r.stdout
    fixed = (tmp_path / path).read_text(encoding="utf-8")
    assert (
        "make (DP_SPECTROGRAM_POWER, x1, x2, DP_PSD_WINDOW_KAISER);" in fixed
    )


def test_a_helper_mentioned_in_a_comment_is_prose(tmp_path: Path) -> None:
    """`the units sections (3, 9, 13)` is a sentence. A helper is file-local
    C, so its rows read the file with comments blanked (TABLE's cannot: a
    header's @code block is a comment)."""
    path = "native/tests/test_spectrogram_core.c"
    src = "/* misread at the units sections (3, 9, 13) */\n"
    src += _helper_files()[path]
    _seed(tmp_path, **{path: src})
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_a_helper_name_in_another_file_is_not_checked(tmp_path: Path) -> None:
    """Helper names are generic (`make`), so a row holds its own file only."""
    src = _NAMED_C.replace("  return !a", "  make (1, 2, 3);\n  return !a")
    _seed(tmp_path, **{"a.c": src})
    assert _run(tmp_path).returncode == 0


def test_a_helper_row_with_no_call_fails(tmp_path: Path) -> None:
    """Fail-closed per helper row, in its file: a helper renamed or inlined
    away must take its row with it. Its definition, every argument a
    declaration, is still there and is not a call."""
    path = "native/validation/spectrogram_certify.c"
    src = _helper_files()[path].replace("  make (", "  remade (")
    _seed(tmp_path, **{path: src})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert f"found no call of the helper make in {path}" in r.stdout


def test_a_table_prototype_is_not_a_call(tmp_path: Path) -> None:
    """A prototype is not a call, so it cannot stand in for a missing one:
    with every dp_acc_trace_create call gone, its row fails although the
    seeded header still declares it."""
    no_t = _NAMED_C.replace(
        "dp_acc_trace_create (8, ACC_TRACE_MEAN, 0.1)", "NULL"
    )
    proto = _NAMED_H_OK + (
        "dp_acc_trace_state_t *dp_acc_trace_create (size_t n, int mode,\n"
        "                                           double alpha);\n"
    )
    _seed(tmp_path, **{"a.c": no_t, "inc/s.h": proto})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "found no dp_acc_trace_create call" in r.stdout


_CERTIFY = "native/validation/spectrogram_certify.c"


def test_a_definition_with_complex_parameters_is_not_a_call(
    tmp_path: Path,
) -> None:
    """`const float _Complex *x` is a declaration too: a definition shaped
    like row_of's may not stand in for its renamed calls (review of #2196:
    the old two-word rule read it as a call and kept the row green)."""
    src = _helper_files()[_CERTIFY]
    src = src.replace(
        "static int row_of (int a0, int a1, int a2);",
        "static void row_of (int window, const float _Complex *x,\n"
        "                    float *restrict row);",
    ).replace("  row_of (", "  rowed (")
    _seed(tmp_path, **{_CERTIFY: src})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert f"found no call of the helper row_of in {_CERTIFY}" in r.stdout


def test_an_unnamed_prototype_is_not_a_call(tmp_path: Path) -> None:
    """A prototype's parameters may be bare types; it is still no call."""
    src = _helper_files()[_CERTIFY]
    src = src.replace(
        "static int make (int a0, int a1, int a2, int a3);",
        "static void make (int, size_t, unsigned long long, const int);",
    ).replace("  make (", "  remade (")
    _seed(tmp_path, **{_CERTIFY: src})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert f"found no call of the helper make in {_CERTIFY}" in r.stdout


def test_a_member_call_is_not_a_helper_call(tmp_path: Path) -> None:
    """`ops->make (...)` is another function: not an arity finding, not a
    literal to fix (review of #2196)."""
    src = _helper_files()[_CERTIFY].replace(
        "void seeded (void)\n{\n",
        "void seeded (void)\n{\n  ops->make (x0, x1, x2, 3);\n"
        "  ops->make (1, 2);\n  s.make (1, 2, 3, 3);\n",
    )
    _seed(tmp_path, **{_CERTIFY: src})
    r = _run(tmp_path, "--fix")
    assert r.returncode == 0, r.stdout
    assert (tmp_path / _CERTIFY).read_text(encoding="utf-8") == src


def test_a_renamed_helper_says_rename_the_row(tmp_path: Path) -> None:
    """Dropping a row un-gates the helper, so a rename is told to rename
    the row, and only a helper that is gone may lose it."""
    src = _helper_files()[_CERTIFY].replace("  make (", "  remade (")
    _seed(tmp_path, **{_CERTIFY: src})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "rename the row, or drop it if the helper is gone" in r.stdout


def test_an_arity_change_prints_before_the_row_it_empties(
    tmp_path: Path,
) -> None:
    """When a helper's only call has another argument count, the row finds
    no call: the arity finding is the cause and must print, not be hidden
    behind the empty row; the footer names HELPERS."""
    src = _helper_files()[_CERTIFY].replace(
        "  make (DP_SPECTROGRAM_POWER, x1, x2, DP_PSD_WINDOW_HANN);",
        "  make (DP_SPECTROGRAM_POWER, x1, DP_PSD_WINDOW_HANN);",
    )
    _seed(tmp_path, **{_CERTIFY: src})
    r = _run(tmp_path)
    assert r.returncode == 1
    out = r.stdout
    arity = out.find("make called with 3 argument(s), its row says 4")
    empty = out.find(f"found no call of the helper make in {_CERTIFY}")
    assert 0 <= arity < empty, out
    assert "TABLE or HELPERS" in out
