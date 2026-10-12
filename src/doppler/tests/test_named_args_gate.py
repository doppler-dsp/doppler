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
  printf ("dp_psd_create(%d, 0, 0, 0, 0, 0, 0, 0, 0) is no call\\n", 9);
  return !a || !b || !p || !q || !c;
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
    # five calls in a.c (the printf is a string), the declaration and the
    # @code call in s.h, the fence in p.md; the prose mention is not a call
    assert "8 call(s) in 3 file(s)" in r.stdout


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
            "ACC_TRACE_MEAN, 0.1",
            "3, 0.1",
            "a.c:9: dp_psd_create mode = 3 (write ACC_TRACE_MINHOLD)",
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
    assert named in _NAMED_C
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


@pytest.mark.parametrize("tree", ["docs/c-api/x.md", "example-projects/q/m.c"])
def test_the_skipped_trees_are_not_the_gates(
    tmp_path: Path, tree: str
) -> None:
    """docs/c-api is generated from the headers already scanned; the example
    projects build against a released doppler that predates the names."""
    _seed(
        tmp_path, **{tree: "x = dp_spectrogram_create (8, 4, 0, 0.0f, 0);\n"}
    )
    assert _run(tmp_path).returncode == 0


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
