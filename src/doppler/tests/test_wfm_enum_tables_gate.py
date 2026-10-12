"""The waveform enum-table gate, exercised over a seeded tree.

`scripts/check_wfm_enum_tables.py` exists because a name table's ORDER is the
C enum value. A second copy that drifts does not fail to compile and does not
raise -- it maps a flag to the wrong waveform. Two instances had already landed
before doppler#760 went looking: `wfm_writer`'s `TYPE_NAMES` fell to 8 entries
with no "dsss", and `wfm_json.c` held the `--data` sources in the REVERSE of
`wfmgen.c`'s order, harmless only because one file compared its lookup result
while the other assigned it.

The cases below seed a fake tree rather than sabotaging the real one, because
a gate that can only be tested against `native/` cannot be tested twice: you
would have to break doppler to check it. Each case is one rule of the gate,
and each asserts the gate goes RED -- a gate that has only ever passed has not
been tested.

Design: docs/design/waveform-enum-ssot.md
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import header_root, repo_root

#: Where the gate looks for headers, relative to the root it is given (jm
#: schema 8): the same answer the gate reads, from scripts/_layout.py.
_HDR = header_root().relative_to(repo_root())

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_wfm_enum_tables.py"

#: A minimal, self-consistent header: two tables, one of each annotation
#: shape, plus the count macro and the C enum the second one names.
_HEADER = """\
/*
 * A banner that MENTIONS the `SSOT:` marker mid-sentence, because the real
 * header's does when it explains the convention, and that must not be read
 * as an annotation for the first table.
 */
#ifndef WFM_NAMES_H
#define WFM_NAMES_H

/* SSOT: enum=wfm_type, count=N_TYPES */
static const char *const TYPE_NAMES[] = { "tone", "noise", "pn" };
#define N_TYPES 3

/* SSOT: enum=ftype, cenum=PKG/wfm_writer/wfm_writer_core.h:wfm_filetype_t */
static const char *const FTYPE_NAMES[] = { "raw", "csv" };

#endif
"""
_HEADER = _HEADER.replace("PKG/", f"{_HDR.name}/")

_MANIFEST = """\
[[enum]]
name = "wfm_type"
values = ["tone", "noise", "pn"]

[[enum]]
name = "ftype"
values = ["raw", "csv"]
"""

_CENUM = """\
typedef enum {
    WFM_FT_RAW = 0, /**< no header. */
    WFM_FT_CSV = 1  /**< text. */
} wfm_filetype_t;
"""


def _seed(
    tmp_path: Path,
    header: str = _HEADER,
    manifest: str = _MANIFEST,
    cenum: str = _CENUM,
    sources: dict[str, str] | None = None,
    headers: dict[str, str] | None = None,
) -> None:
    names = tmp_path / _HDR / "wfm" / "wfm_names.h"
    names.parent.mkdir(parents=True, exist_ok=True)
    names.write_text(header, encoding="utf-8")

    writer = tmp_path / _HDR / "wfm_writer" / "wfm_writer_core.h"
    writer.parent.mkdir(parents=True, exist_ok=True)
    writer.write_text(cenum, encoding="utf-8")

    (tmp_path / "just-makeit.toml").write_text(manifest, encoding="utf-8")

    for rel, body in (sources or {}).items():
        p = tmp_path / "native" / "src" / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")

    for rel, body in (headers or {}).items():
        p = tmp_path / _HDR / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")


#: Rule 7's fixture: an [[enum]] bound BY NAME and the one C enum carrying it.
_BOUND_H = """\
/** The window. A doc block above the annotation, as the real headers have. */
/* SSOT: enum=win */
enum {
    W_HANN = 0, /**< Hann; a `;` in a comment ends nothing. */
    W_RECT = 1  /**< rect. */
};
"""

_BOUND_TOML = """
[[enum]]
name = "win"
values = ["hann", "rect"]
enumerators = ["W_HANN", "W_RECT"]
"""


def _seed_bound(
    tmp_path: Path, header: str = _BOUND_H, manifest: str = _BOUND_TOML
) -> None:
    _seed(
        tmp_path,
        manifest=_MANIFEST + manifest,
        headers={"psd/win.h": header},
    )


def _run(tmp_path: Path, *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path), *extra],
        capture_output=True,
        text=True,
    )


def _git(tmp_path: Path, *args: str) -> None:
    subprocess.run(
        [
            "git",
            "-C",
            str(tmp_path),
            "-c",
            "user.email=t@t",
            "-c",
            "user.name=t",
            *args,
        ],
        check=True,
        capture_output=True,
    )


def test_a_consistent_tree_passes(tmp_path: Path) -> None:
    """The baseline. Without it every RED below proves nothing."""
    _seed(tmp_path)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "2 table(s)" in r.stdout


def test_a_reordered_table_fails(tmp_path: Path) -> None:
    """Rule 2, the whole point: order IS the value."""
    _seed(
        tmp_path,
        header=_HEADER.replace(
            '{ "tone", "noise", "pn" }', '{ "noise", "tone", "pn" }'
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    # Both sides, because "they differ" alone costs a round trip to act on.
    assert "TYPE_NAMES[] != [[enum]] wfm_type" in r.stdout
    assert "'noise', 'tone', 'pn'" in r.stdout
    assert "'tone', 'noise', 'pn'" in r.stdout


def test_a_dropped_entry_fails(tmp_path: Path) -> None:
    """The `wfm_writer` rot that actually shipped, replayed."""
    _seed(
        tmp_path,
        header=_HEADER.replace(
            '{ "tone", "noise", "pn" }', '{ "tone", "noise" }'
        ).replace("#define N_TYPES 3", "#define N_TYPES 2"),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "TYPE_NAMES[] != [[enum]] wfm_type" in r.stdout


def test_an_unannotated_table_fails(tmp_path: Path) -> None:
    """Rule 3: adding a table is the moment to say which [[enum]] owns it."""
    _seed(
        tmp_path,
        header=_HEADER.replace(
            "#endif",
            'static const char *const NEW_NAMES[] = { "a", "b" };\n#endif',
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "NEW_NAMES[] carries no `SSOT:` annotation" in r.stdout


def test_an_annotation_naming_no_such_enum_fails(tmp_path: Path) -> None:
    _seed(tmp_path, header=_HEADER.replace("enum=wfm_type", "enum=nope"))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "no [[enum]] named 'nope'" in r.stdout


def test_a_drifted_count_macro_fails(tmp_path: Path) -> None:
    _seed(
        tmp_path,
        header=_HEADER.replace("#define N_TYPES 3", "#define N_TYPES 4"),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "N_TYPES is 4, but TYPE_NAMES[] has 3 entries" in r.stdout


def test_a_c_enum_value_off_its_index_fails(tmp_path: Path) -> None:
    """Rule 4: the enumerator that pins the ordering must agree with it."""
    _seed(tmp_path, cenum=_CENUM.replace("WFM_FT_CSV = 1", "WFM_FT_CSV = 7"))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "WFM_FT_CSV = 7, but it is at index 1" in r.stdout


def test_a_missing_c_enum_fails_rather_than_skipping(tmp_path: Path) -> None:
    """A check that cannot run has not passed."""
    _seed(tmp_path, cenum="/* the typedef was renamed away */\n")
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "wfm_filetype_t" in r.stdout
    assert "checked by nothing" in r.stdout


def test_a_second_c_home_fails(tmp_path: Path) -> None:
    """Rule 1: the regression this gate exists to catch."""
    _seed(
        tmp_path,
        sources={
            "app/wfmgen.c": (
                "static const char *const TYPES[]"
                ' = { "tone", "noise", "pn" };\n'
            )
        },
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "TYPES[] re-declares TYPE_NAMES[]" in r.stdout


def test_a_generated_binding_is_not_called_a_duplicate(tmp_path: Path) -> None:
    """jm renders `_enum_*` tables from the SAME [[enum]] blocks.

    Reporting those would be reporting the manifest's own output as a copy of
    the manifest -- noise that would make the gate's real findings unreadable.
    """
    _seed(
        tmp_path,
        sources={
            "wfm_compose/wfm_compose_ext.c": (
                "static const char *const _enum_wfm_type[]"
                ' = { "tone", "noise", "pn" };\n'
            )
        },
    )
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_a_stale_binding_table_fails(tmp_path: Path) -> None:
    """Rule 6: not being a duplicate is not the same as being correct.

    The case that put this rule here (2026-09-03): a third value was added to
    the `t0_source` enum, and the manifest, the C enum and the `.pyi` all took
    it while the SACRED `wfm_reader_ext_wfm_reader.c` fragment did not -- jm
    reconciles such a fragment member by member and never re-renders its
    tables. The getter then indexed one past the array's own NULL for the new
    value, which is a read of whatever follows it rather than a wrong string.
    """
    _seed(
        tmp_path,
        sources={
            "wfm_reader/wfm_reader_ext_wfm_reader.c": (
                "static const char *const _enum_Reader_wfm_type[]"
                ' = { "tone", "noise", NULL };\n'
            )
        },
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "_enum_Reader_wfm_type[] is ['tone', 'noise']" in r.stdout
    assert "NOT re-rendered" in r.stdout


def test_a_reordered_binding_table_fails(tmp_path: Path) -> None:
    """Same length, wrong order — the half a length check cannot see."""
    _seed(
        tmp_path,
        sources={
            "wfm_reader/wfm_reader_ext_wfm_reader.c": (
                "static const char *const _enum_Reader_ftype[]"
                ' = { "csv", "raw", NULL };\n'
            )
        },
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "_enum_Reader_ftype[]" in r.stdout


def test_a_binding_table_naming_no_enum_fails(tmp_path: Path) -> None:
    """Fail-closed: a table this gate cannot resolve is not a table it may
    quietly skip, or a rename becomes a way out of the check."""
    _seed(
        tmp_path,
        sources={
            "wfm_reader/wfm_reader_ext_wfm_reader.c": (
                "static const char *const _enum_Reader_whence[]"
                ' = { "tone", "noise", "pn", NULL };\n'
            )
        },
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "matches no [[enum]]" in r.stdout


def test_a_manifest_side_edit_fails(tmp_path: Path) -> None:
    """The declaration can drift from the header just as the header can."""
    _seed(
        tmp_path,
        manifest=_MANIFEST.replace(
            'values = ["raw", "csv"]', 'values = ["csv", "raw"]'
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "FTYPE_NAMES[] != [[enum]] ftype" in r.stdout


def test_an_emptied_header_is_not_a_pass(tmp_path: Path) -> None:
    """A matcher that finds nothing reports a clean tree.

    This is the failure mode the repo keeps re-learning: the scanned shape
    changes, discovery matches nothing, and full coverage of an empty set
    reads as green.
    """
    _seed(
        tmp_path, header="#ifndef WFM_NAMES_H\n#define WFM_NAMES_H\n#endif\n"
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "no name tables found" in r.stdout


def test_a_bound_c_enum_passes(tmp_path: Path) -> None:
    """Rule 7's baseline, so each RED below is about its one edit."""
    _seed_bound(tmp_path)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "1 C enum(s) bound by name" in r.stdout


def test_a_c_value_with_no_python_name_fails(tmp_path: Path) -> None:
    """Rule 7, the direction the compiler cannot see: jm writes each
    manifest name as a C constant, so a missing C name fails to build, but a
    C value the manifest never names compiles and is unreachable from
    Python."""
    _seed_bound(
        tmp_path,
        header=_BOUND_H.replace("W_RECT = 1", "W_KAISER = 1,\n    W_RECT = 2"),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "the C enum's names != [[enum]] win enumerators" in r.stdout
    assert "'W_HANN', 'W_KAISER', 'W_RECT'" in r.stdout


def test_enumerators_no_c_enum_carries_fail(tmp_path: Path) -> None:
    """Fail-closed: binding by name is a claim about ONE C enum."""
    _seed_bound(tmp_path, header=_BOUND_H.replace("/* SSOT: enum=win */", ""))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "exactly one C enum must carry `SSOT: enum=win`" in r.stdout


def test_an_annotation_does_not_reach_past_a_declaration(
    tmp_path: Path,
) -> None:
    """The annotation belongs to the next declaration, not to the next enum:
    an enum must not adopt one written for something above it."""
    _seed_bound(
        tmp_path,
        header=_BOUND_H.replace(
            "/* SSOT: enum=win */", "/* SSOT: enum=win */\nint x;"
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "exactly one C enum must carry `SSOT: enum=win`" in r.stdout


#: Rule 8's fixture: a view over `win`, and the C comparison that bounds it.
_VIEW_TOML = (
    _BOUND_TOML + '\n[[enum]]\nname = "win_sub"\nvalues = ["hann"]\n'
    'enumerators = ["W_HANN"]\n'
)
_VIEW_C = """\
int make (int window)
{
  /* SSOT: view=win_sub */
  if (s->window > W_HANN)
    return 0;
  return 1;
}
"""


def _seed_view(
    tmp_path: Path, manifest: str = _VIEW_TOML, c: str | None = _VIEW_C
) -> None:
    _seed_bound(tmp_path, manifest=manifest)
    if c is not None:
        f = tmp_path / "native/src/acq/acq_core.c"
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(c, encoding="utf-8")


def test_a_view_over_a_bound_enum_passes(tmp_path: Path) -> None:
    """carrier_acq_window's shape: a subset of a bound enum's names, with no
    C enum of its own, and the C comparison that refuses past it. `->` in
    `s->window` is a member access, not the comparison."""
    _seed_view(tmp_path)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "1 view(s) bounded in C" in r.stdout


def test_a_view_with_no_c_bound_fails(tmp_path: Path) -> None:
    """Rule 8: C takes the parent's type, so a view nothing in C bounds
    lets a C caller pass the parent's other values (#2194)."""
    _seed_view(tmp_path, c=None)
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "view [[enum]] win_sub has no C refusal" in r.stdout
    # the message says where the annotation goes and what to write
    assert "`/* SSOT: view=win_sub */` on the line above" in r.stdout
    assert "`if (<arg> > W_HANN)` (or `>= W_RECT`)" in r.stdout


def test_a_c_bound_naming_another_enumerator_fails(tmp_path: Path) -> None:
    """Moving the refusal without the view (or the view without the
    refusal) makes the C and Python sets differ."""
    _seed_view(tmp_path, c=_VIEW_C.replace("> W_HANN", "> W_RECT"))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert (
        "the refusal `> W_RECT` admits ['W_HANN', 'W_RECT'], but view"
        " win_sub is ['W_HANN']: write `> W_HANN` (or `>= W_RECT`)"
    ) in r.stdout


def test_a_view_that_is_not_a_prefix_fails(tmp_path: Path) -> None:
    """One comparison bounds a prefix of the parent, nothing else."""
    _seed_view(
        tmp_path,
        manifest=_VIEW_TOML.replace(
            'values = ["hann"]\nenumerators = ["W_HANN"]',
            'values = ["rect"]\nenumerators = ["W_RECT"]',
        ),
        c=_VIEW_C.replace("> W_HANN", "> W_RECT"),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "is not a prefix of win" in r.stdout


def test_a_view_annotation_naming_no_view_fails(tmp_path: Path) -> None:
    _seed_view(
        tmp_path,
        c=_VIEW_C + "/* SSOT: view=nonesuch */\nif (w > W_HANN) {}\n",
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "`SSOT: view=nonesuch` names no view" in r.stdout


def test_a_view_with_a_name_of_its_own_fails(tmp_path: Path) -> None:
    _seed_view(
        tmp_path,
        manifest=_BOUND_TOML
        + '\n[[enum]]\nname = "win_sub"\nvalues = ["hann", "x"]\n'
        'enumerators = ["W_HANN", "W_OTHER"]\n',
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "[[enum]] win_sub lists `enumerators`" in r.stdout


def test_an_annotated_enum_bound_by_position_fails(tmp_path: Path) -> None:
    """An annotation on an [[enum]] with no `enumerators` claims a tie that
    nothing makes: jm numbers those names by position."""
    _seed_bound(
        tmp_path,
        manifest=_BOUND_TOML.replace(
            'enumerators = ["W_HANN", "W_RECT"]\n', ""
        ),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "[[enum]] win lists no `enumerators`" in r.stdout


def test_an_annotation_naming_no_enum_fails(tmp_path: Path) -> None:
    _seed_bound(tmp_path, header=_BOUND_H.replace("enum=win", "enum=nope"))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "`SSOT: enum=nope` names no [[enum]]" in r.stdout


def test_a_c_enum_annotation_with_table_fields_fails(tmp_path: Path) -> None:
    _seed_bound(
        tmp_path,
        header=_BOUND_H.replace("enum=win */", "enum=win, count=N_WIN */"),
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "takes `enum=` only" in r.stdout


def test_a_renumbered_enumerator_fails_in_the_tree(tmp_path: Path) -> None:
    """Rule 7's values: a bound value is persisted (the Spectrogram blob
    stores its window), so `KAISER = 5` re-reads every saved blob -- and the
    names, which still match, cannot see it."""
    _seed_bound(tmp_path, header=_BOUND_H.replace("W_RECT = 1", "W_RECT = 5"))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "values [0, 5] do not run consecutively" in r.stdout


def test_a_duplicate_value_fails(tmp_path: Path) -> None:
    _seed_bound(tmp_path, header=_BOUND_H.replace("W_RECT = 1", "W_RECT = 0"))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "values [0, 0] do not run consecutively" in r.stdout


def _seed_bound_git(tmp_path: Path) -> None:
    """The bound fixture as a repository whose HEAD is the base."""
    _seed_bound(tmp_path)
    _git(tmp_path, "init", "-q")
    _git(tmp_path, "add", ".")
    _git(tmp_path, "commit", "-q", "-m", "base")


def test_an_unchanged_value_passes_against_the_base(tmp_path: Path) -> None:
    """The ratchet's baseline, in a real repository with a real base."""
    _seed_bound_git(tmp_path)
    r = _run(tmp_path, "--base", "HEAD")
    assert r.returncode == 0, r.stdout


def test_a_renumber_with_a_matching_reorder_fails_against_the_base(
    tmp_path: Path,
) -> None:
    """The case the tree cannot see: the C enum renumbered AND the manifest
    reordered to match, so names, order and consecutiveness all hold. Only
    the merge base remembers that W_HANN was 0."""
    _seed_bound_git(tmp_path)
    (tmp_path / _HDR / "psd" / "win.h").write_text(
        "/* SSOT: enum=win */\nenum {\n    W_RECT = 0,\n    W_HANN = 1\n};\n",
        encoding="utf-8",
    )
    (tmp_path / "just-makeit.toml").write_text(
        _MANIFEST
        + _BOUND_TOML.replace('["hann", "rect"]', '["rect", "hann"]').replace(
            '["W_HANN", "W_RECT"]', '["W_RECT", "W_HANN"]'
        ),
        encoding="utf-8",
    )
    r = _run(tmp_path, "--base", "HEAD")
    assert r.returncode == 1
    assert "W_HANN was 0 at the merge base" in r.stdout
    assert "W_RECT was 1 at the merge base" in r.stdout


def test_an_unreadable_base_fails_closed(tmp_path: Path) -> None:
    """A shallow clone has a baseline it cannot read: that is a failure that
    names the ref, never a silent pass (the alloc ratchet's #1051)."""
    _seed_bound_git(tmp_path)
    r = _run(tmp_path, "--base", "refs/heads/no-such-base")
    assert r.returncode == 1
    assert "`refs/heads/no-such-base` does not resolve" in r.stdout


def test_the_real_tree_passes() -> None:
    """The gate against doppler itself, so the seeded cases are not the only
    thing it has ever been pointed at.

    `--base HEAD` deliberately neuters rule 7's values ratchet here. Whether
    an enumerator's value moved since main is a question about a BRANCH, and
    it has an execution home: `make lint-wfm-enum-tables`, which the
    `wfm-enum-tables` pre-commit hook dispatches to at the default
    `--base origin/main`, under the `pre-commit (lint + format)` job that
    checks out at `fetch-depth: 0`. This test asks the other question -- does
    the tree agree with its manifest -- and runs in the Python jobs, whose
    shallow checkout has no `origin/main`: left at the default it fails
    closed there, testing the fetch depth instead of the gate (#2190's first
    CI run). Same shape as test_alloc_helper_gate.py.
    """
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--base", "HEAD"],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout


def test_a_doubled_annotation_fails(tmp_path: Path) -> None:
    """Exactly one refusal states a view's bound: two is as wrong as none
    (weakening `!= 1` to `< 1` must not pass)."""
    _seed_view(tmp_path, c=_VIEW_C + _VIEW_C.replace("make", "make2"))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "has 2 `SSOT: view=win_sub` annotations" in r.stdout


def test_the_equivalent_ge_form_passes(tmp_path: Path) -> None:
    """`>= NEXT` is the same bound as `> LAST`: the likeliest rewrite."""
    _seed_view(tmp_path, c=_VIEW_C.replace("> W_HANN", ">= W_RECT"))
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_an_operand_with_arithmetic_is_no_refusal(tmp_path: Path) -> None:
    """`> W_HANN + 1` admits a value past the view: the operand must be the
    enumerator alone."""
    _seed_view(tmp_path, c=_VIEW_C.replace("> W_HANN", "> W_HANN + 1"))
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "is not a refusal of the form `> W_HANN`" in r.stdout


def test_a_line_comment_annotation_is_read(tmp_path: Path) -> None:
    """`// SSOT: view=X` ends at its line, not at the next `*/` in the file
    (a later block comment must not move the scan)."""
    c = (
        _VIEW_C.replace(
            "  /* SSOT: view=win_sub */", "  // SSOT: view=win_sub"
        )
        + "/* a later comment */\n"
    )
    _seed_view(tmp_path, c=c)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_the_refusal_must_be_the_next_line(tmp_path: Path) -> None:
    """The annotation sits directly above the refusal: one statement in
    between and it describes nothing."""
    c = _VIEW_C.replace(
        "  /* SSOT: view=win_sub */\n",
        "  /* SSOT: view=win_sub */\n  int unrelated = 0;\n",
    )
    _seed_view(tmp_path, c=c)
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "the line after `SSOT: view=win_sub` is not a refusal" in r.stdout
