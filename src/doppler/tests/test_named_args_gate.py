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

import re
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


#: A seeded file of helpers, as the tests and harnesses write them: `make`
#: forwards both slots into a constructor, `wrap` forwards through `make` (a
#: second hop), `row_of` forwards its window past declarations a two-word
#: rule misread as a call's arguments (the certification harness's shape,
#: review of #2196), `is_kaiser` only compares, and `scale` forwards inside
#: an expression -- the last two are no helpers.
_HELPERS_REL = "native/tests/helpers.c"
_HELPERS_C = """\
static dp_spectrogram_state_t *
make (int mode, size_t nfft, size_t hop, int window)
{
  return dp_spectrogram_create (nfft, hop, window, 0.0f, mode);
}

static void
row_of (int window, const float _Complex *x,
        float *restrict row)
{
  dp_spectrogram_state_t *s
      = dp_spectrogram_create (8, 4, window, 0.0f, DP_SPECTROGRAM_POWER);
  (void)x;
  (void)row;
  (void)s;
}

static dp_spectrogram_state_t *
wrap (int w)
{
  return make (DP_SPECTROGRAM_POWER, 8, 4, w);
}

static int
is_kaiser (int window)
{
  return window == DP_PSD_WINDOW_KAISER;
}

static dp_spectrogram_state_t *
scale (int w)
{
  return dp_spectrogram_create (8, 4, w + 0, 0.0f, DP_SPECTROGRAM_POWER);
}

void
seeded (void)
{
  make (DP_SPECTROGRAM_DB, 8, 4, DP_PSD_WINDOW_HANN);
  wrap (DP_PSD_WINDOW_RECT);
  row_of (DP_PSD_WINDOW_KAISER, NULL, NULL);
  is_kaiser (1);
  scale (1);
}
"""


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


#: carrier_acq's window is a VIEW of psd_window: three of its four.
_VIEW_TOML = """\
[[enum]]
name = "psd_window"
values = ["hann", "kaiser", "blackman-harris", "rect"]
enumerators = ["DP_PSD_WINDOW_HANN", "DP_PSD_WINDOW_KAISER",
               "DP_PSD_WINDOW_BLACKMAN_HARRIS", "DP_PSD_WINDOW_RECT"]

[[enum]]
name = "carrier_acq_window"
values = ["hann", "kaiser", "blackman-harris"]
enumerators = ["DP_PSD_WINDOW_HANN", "DP_PSD_WINDOW_KAISER",
               "DP_PSD_WINDOW_BLACKMAN_HARRIS"]
"""


def _seed(tmp_path: Path, **override: str) -> None:
    files = {
        "just-makeit.toml": _VIEW_TOML,
        f"{_HDR}/psd/psd_core.h": _ENUMS_H,
        "a.c": _NAMED_C,
        "inc/s.h": _NAMED_H_OK,
        "docs/p.md": _NAMED_MD,
        _HELPERS_REL: _HELPERS_C,
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
    # Direct: seven calls in a.c (the printf is a string), the @code call in
    # s.h (its declaration is not a call), the fence in p.md, and the three
    # in helpers.c's bodies (make's, row_of's, scale's). Helpers: make twice
    # (seeded and wrap's body), wrap once and row_of once; is_kaiser and
    # scale are discovered as no helpers, and the definitions are no calls.
    assert (
        "12 call(s) and 4 call(s) of 3 discovered helper(s) in 4 file(s)"
        in r.stdout
    ), r.stdout


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


def test_example_projects_hold_the_window_slot_too(tmp_path: Path) -> None:
    """The example projects build against a released doppler, and v0.67.0
    carries the DP_PSD_WINDOW_* names (#2186), so their window slot is held
    like every other: a name passes, and a number fails with its line."""
    ok = (
        "x = dp_spectrogram_create (8, 4, DP_PSD_WINDOW_HANN, 0.0f,\n"
        "                           DP_SPECTROGRAM_POWER);\n"
    )
    _seed(tmp_path, **{"example-projects/q/m.c": ok})
    assert _run(tmp_path).returncode == 0
    bad = "x = dp_spectrogram_create (8, 4, 0, 0.0f, DP_SPECTROGRAM_POWER);\n"
    _seed(tmp_path / "bad", **{"example-projects/q/m.c": bad})
    r = _run(tmp_path / "bad")
    assert r.returncode == 1
    assert (
        "example-projects/q/m.c:1: dp_spectrogram_create window = 0 "
        "(write DP_PSD_WINDOW_HANN)" in r.stdout
    ), r.stdout


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


def _helpers(old: str, new: str) -> dict[str, str]:
    assert _HELPERS_C.count(old) == 1, old
    return {_HELPERS_REL: _HELPERS_C.replace(old, new)}


def _finding(r: subprocess.CompletedProcess[str], msg: str) -> None:
    """`msg` is reported against the helpers file and a line in it: a
    finding that names no file cannot be found in a tree of 900."""
    where = rf"^  {re.escape(_HELPERS_REL)}:\d+: {re.escape(msg)}$"
    assert re.search(where, r.stdout, re.MULTILINE), r.stdout


def test_a_forwarding_helper_is_discovered_and_held(tmp_path: Path) -> None:
    """`make` forwards both slots into a constructor, so its literals are
    refused like a direct call's (#2191), with no table to keep (#2197)."""
    _seed(
        tmp_path,
        **_helpers(
            "  make (DP_SPECTROGRAM_DB, 8, 4, DP_PSD_WINDOW_HANN);",
            "  make (1, 8, 4, 3);",
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    _finding(r, "make mode = 1 (write DP_SPECTROGRAM_DB)")
    _finding(r, "make window = 3 (write DP_PSD_WINDOW_RECT)")


def test_a_second_hop_is_discovered(tmp_path: Path) -> None:
    """`wrap` forwards through `make`: found at the fixpoint."""
    _seed(tmp_path, **_helpers("  wrap (DP_PSD_WINDOW_RECT);", "  wrap (1);"))
    r = _run(tmp_path)
    assert r.returncode == 1
    _finding(r, "wrap window = 1 (write DP_PSD_WINDOW_KAISER)")


def test_a_helper_with_declarator_parameters_is_held(tmp_path: Path) -> None:
    """`row_of (int window, const float _Complex *x, float *restrict row)`
    is the certification harness's shape. Its definition is declarations,
    not a call (review of #2196: a two-word rule read it as one, so the
    baseline counts one row_of call, the seeded one), and it forwards
    `window`, so a literal at its call is refused in its own file."""
    _seed(
        tmp_path,
        **_helpers(
            "  row_of (DP_PSD_WINDOW_KAISER, NULL, NULL);",
            "  row_of (1, NULL, NULL);",
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    _finding(r, "row_of window = 1 (write DP_PSD_WINDOW_KAISER)")


@pytest.mark.parametrize(
    "prefix",
    [
        "JM_HOT static",
        "[[maybe_unused]] static",
        "static __attribute__((unused))",
        "  static",
    ],
    ids=["macro", "c23-attribute", "gnu-attribute", "indented"],
)
def test_a_prefixed_or_indented_definition_is_discovered(
    tmp_path: Path, prefix: str
) -> None:
    """`JM_HOT static` (this tree's own idiom, fir_core.c), a C23 or a GNU
    attribute, and an indented `static` all define a helper. A shape missed
    drops `make`, and every helper that calls it, while the gate still says
    OK: so a literal at `make` must stay red in each."""
    files = _helpers(
        "static dp_spectrogram_state_t *\nmake (",
        f"{prefix} dp_spectrogram_state_t *\nmake (",
    )
    files[_HELPERS_REL] = files[_HELPERS_REL].replace(
        "  make (DP_SPECTROGRAM_DB, 8, 4, DP_PSD_WINDOW_HANN);",
        "  make (DP_SPECTROGRAM_DB, 8, 4, 3);",
    )
    _seed(tmp_path, **files)
    r = _run(tmp_path)
    assert r.returncode == 1, r.stdout
    _finding(r, "make window = 3 (write DP_PSD_WINDOW_RECT)")


def test_fix_names_a_helper_literal(tmp_path: Path) -> None:
    _seed(tmp_path, **_helpers("  wrap (DP_PSD_WINDOW_RECT);", "  wrap (3);"))
    r = _run(tmp_path, "--fix")
    assert r.returncode == 0, r.stdout
    assert "named 1 literal(s)" in r.stdout
    fixed = (tmp_path / _HELPERS_REL).read_text(encoding="utf-8")
    assert "  wrap (DP_PSD_WINDOW_RECT);" in fixed


def test_a_compare_or_an_expression_is_no_helper(tmp_path: Path) -> None:
    """`is_kaiser` only compares and `scale` forwards `w + 0`: neither is
    discovered, so their literals are not this gate's (the baseline's
    `is_kaiser (1)` and `scale (1)` pass)."""
    _seed(
        tmp_path,
        **_helpers(
            "  is_kaiser (1);\n  scale (1);", "  is_kaiser (3);\n  scale (3);"
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "of 3 discovered helper(s)" in r.stdout


def test_a_non_static_function_is_no_helper(tmp_path: Path) -> None:
    """A helper is file-local; a library's own entry points are TABLE
    rows. A non-`static` forwarder is not discovered (a stated limit)."""
    pub = (
        "dp_spectrogram_state_t *\npub (int window)\n{\n"
        "  return dp_spectrogram_create (8, 4, window, 0.0f,"
        " DP_SPECTROGRAM_POWER);\n}\nvoid g (void) { pub (3); }\n"
    )
    _seed(tmp_path, **{"native/tests/pub.c": pub})
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "of 3 discovered helper(s)" in r.stdout


def test_discovering_nothing_fails_closed(tmp_path: Path) -> None:
    """A tree whose discovery finds no helper has a broken discovery, not
    a clean tree: the same rule as a TABLE row with no call."""
    _seed(tmp_path, **{_HELPERS_REL: "void nothing (void) {}\n"})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "discovered no forwarding helper" in r.stdout


def test_a_helper_mentioned_in_a_comment_is_prose(tmp_path: Path) -> None:
    """`the units sections (3, 9, 13)` is a sentence: a helper's rows read
    the file with comments blanked (TABLE's cannot: a header's @code block
    is a comment)."""
    _seed(
        tmp_path,
        **{_HELPERS_REL: "/* compare make (3, 9) */\n" + _HELPERS_C},
    )
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_a_helper_name_in_another_file_is_not_checked(tmp_path: Path) -> None:
    """Helper names are generic (`make`), so a row holds its own file only."""
    src = _NAMED_C.replace("  return !a", "  make (1, 2, 3);\n  return !a")
    _seed(tmp_path, **{"a.c": src})
    assert _run(tmp_path).returncode == 0


def test_a_member_call_is_not_a_helper_call(tmp_path: Path) -> None:
    """`ops->make (...)` and `s.make (...)` are other functions: not an
    arity finding, not a literal to fix (review of #2196)."""
    files = _helpers(
        "  scale (1);\n",
        "  scale (1);\n  ops->make (x, 1, 2, 3);\n  ops->make (1, 2);\n"
        "  s.make (1, 2, 3, 3);\n",
    )
    _seed(tmp_path, **files)
    r = _run(tmp_path, "--fix")
    assert r.returncode == 0, r.stdout
    got = (tmp_path / _HELPERS_REL).read_text(encoding="utf-8")
    assert got == files[_HELPERS_REL]


def test_definitions_of_any_shape_are_not_calls(tmp_path: Path) -> None:
    """`const size_t`, `unsigned long long`, the seed's `row_of` with its
    `float _Complex *restrict`, and an unnamed prototype whose parameters
    are bare multi-word types, `(int, size_t, unsigned long long, const
    int)`, are declarations: the counts do not move, and `make` is still
    discovered by its named parameters."""
    files = _helpers(
        "static dp_spectrogram_state_t *\nmake (int mode, size_t nfft,"
        " size_t hop, int window)\n",
        "static dp_spectrogram_state_t *make (int, size_t,"
        " unsigned long long, const int);\n"
        "static dp_spectrogram_state_t *\nmake (int mode, const size_t nfft,"
        " unsigned long long hop, int window)\n",
    )
    _seed(tmp_path, **files)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "12 call(s) and 4 call(s) of 3 discovered helper(s)" in r.stdout, (
        r.stdout
    )


def test_an_arity_change_is_reported(tmp_path: Path) -> None:
    """A helper call with another argument count is a finding, against its
    file, and the footer says whose row it is."""
    _seed(
        tmp_path,
        **_helpers(
            "  make (DP_SPECTROGRAM_DB, 8, 4, DP_PSD_WINDOW_HANN);",
            "  make (DP_SPECTROGRAM_DB, 8, DP_PSD_WINDOW_HANN);",
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    _finding(r, "make called with 3 argument(s), its row says 4")
    assert "discovered helper's" in r.stdout


def test_an_arity_change_prints_before_the_row_it_empties(
    tmp_path: Path,
) -> None:
    """When a TABLE function's only call has another argument count, its
    row finds no call: the arity finding is the cause and must print first,
    not be hidden behind the empty row it causes."""
    src = _NAMED_C.replace(
        "dp_acc_trace_create (8, ACC_TRACE_MEAN, 0.1)",
        "dp_acc_trace_create (8, ACC_TRACE_MEAN)",
    )
    _seed(tmp_path, **{"a.c": src})
    r = _run(tmp_path)
    assert r.returncode == 1
    out = r.stdout
    arity = out.find(
        "a.c:16: dp_acc_trace_create called with 2 argument(s), its row says 3"
    )
    empty = out.find("found no dp_acc_trace_create call")
    assert 0 <= arity < empty, out


_W = ("window", "psd_window")
_M = ("mode", "spectrogram_mode")


def test_the_real_trees_helpers_are_discovered() -> None:
    """Nine of the ten helpers #2191's hand table listed are found in the
    real tree by data flow, each pinned whole: file, name, argument count,
    and each slot's position, name and enum. A helper discovery stops
    seeing (a definition shape it does not read) or reads differently is
    red here, though the gate itself would still say OK; a NEW helper needs
    no edit, since this is a subset. The tenth, `rows_are_the_psd`, and
    `oracle`'s mode reach only a compare, so a literal mode at their calls
    is no longer refused (#2227)."""
    g = _gate()
    want = {
        "native/tests/test_spectrogram_core.c": {
            "ref_psd": (3, {1: _W}),
            "oneshot": (7, {0: _M, 3: _W}),
            "sections": (1, {0: _M}),
        },
        "native/validation/spectrogram_certify.c": {
            "row_of": (3, {0: _W}),
            "make": (4, {0: _M, 3: _W}),
            "ref_psd": (2, {1: _W}),
            "oracle": (6, {3: _W}),
            "partition": (11, {0: _M, 3: _W}),
        },
        "native/benchmarks/bench_spectrogram_core.c": {"add": (8, {7: _M})},
    }
    for rel, rows in want.items():
        found = g.discover((REPO / rel).read_text(encoding="utf-8"))
        for name, row in rows.items():
            assert found.get(name) == row, (rel, name, found.get(name))


def test_a_value_outside_a_view_has_no_name(tmp_path: Path) -> None:
    """carrier_acq's slot is the carrier_acq_window view: rect (3) is a
    psd_window value the create refuses, so it is no such value here, and
    --fix leaves it rather than writing DP_PSD_WINDOW_RECT (#2194)."""
    bad = _NAMED_C.replace("0.0, 4, DP_PSD_WINDOW_HANN,", "0.0, 4, 3,")
    _seed(tmp_path, **{"a.c": bad})
    r = _run(tmp_path, "--fix")
    assert r.returncode == 1
    assert "dp_carrier_acq_create window = 3 (no such value)" in r.stdout
    assert (tmp_path / "a.c").read_text(encoding="utf-8") == bad


@pytest.mark.parametrize(
    "between", [" /* the create */ ", "\n  // the create\n  "]
)
def test_a_comment_before_the_paren_does_not_hide_a_call(
    tmp_path: Path, between: str
) -> None:
    """`dp_psd_create /* c */ (...)` is still a call, and its literal is
    still refused (#2198)."""
    bad = _NAMED_C.replace(
        "dp_psd_create (64, 1.0, DP_PSD_WINDOW_KAISER",
        f"dp_psd_create{between}(64, 1.0, 1",
    )
    assert bad != _NAMED_C
    _seed(tmp_path, **{"a.c": bad})
    r = _run(tmp_path)
    assert r.returncode == 1, r.stdout
    assert "dp_psd_create window = 1 (write DP_PSD_WINDOW_KAISER)" in r.stdout


def test_a_run_of_comments_before_a_paren_stays_linear(tmp_path: Path) -> None:
    """Forty comments between a name and a `(` -- or no `(` at all -- scan
    in linear time: the comment is a tempered dot, never a lazy `.*?` that
    re.S lets cross into the next comment and backtrack (review of #2202)."""
    run = " ".join(f"/* {i} */" for i in range(40))
    src = _NAMED_C.replace(
        "  return !a", f"  dp_psd_create {run} ;\n  return !a"
    )
    _seed(tmp_path, **{"a.c": src})
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
        timeout=20,
    )
    assert r.returncode == 0, r.stdout
