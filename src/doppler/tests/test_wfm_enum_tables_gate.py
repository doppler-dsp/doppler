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

from doppler.tests._repo import repo_root

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

/* SSOT: enum=ftype, cenum=wfm_writer/wfm_writer_core.h:wfm_filetype_t */
static const char *const FTYPE_NAMES[] = { "raw", "csv" };

#endif
"""

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
) -> None:
    names = tmp_path / "native" / "inc" / "wfm" / "wfm_names.h"
    names.parent.mkdir(parents=True, exist_ok=True)
    names.write_text(header, encoding="utf-8")

    writer = tmp_path / "native" / "inc" / "wfm_writer" / "wfm_writer_core.h"
    writer.parent.mkdir(parents=True, exist_ok=True)
    writer.write_text(cenum, encoding="utf-8")

    (tmp_path / "just-makeit.toml").write_text(manifest, encoding="utf-8")

    for rel, body in (sources or {}).items():
        p = tmp_path / "native" / "src" / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")


def _run(tmp_path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
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


def test_the_real_tree_passes() -> None:
    """The gate against doppler itself, so the seeded cases are not the only
    thing it has ever been pointed at."""
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout
