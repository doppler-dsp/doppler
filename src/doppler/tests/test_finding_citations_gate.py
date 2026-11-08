"""The finding-citation gate, exercised over a seeded tree.

`scripts/check_finding_citations.py` holds #2059's rule outside the
reports, in both directions: a validation finding is never cited by its
number, because a report numbers findings by position and a number
re-points the moment one before it is dropped; and every
``finding `key``` names a key some ``validate.py`` still records, because
a renamed finding would otherwise leave its citation pointing at nothing.
Inside a report the render step refuses a typed number
(`test_validation_report.py`); this gate is the half for every file with
no render step.

The cases seed a small git repository, because the gate reads `git
ls-files` and a gate that can only run against the real tree cannot be
sabotaged without breaking doppler.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_finding_citations.py"

#: The validator whose keys the seeded citations resolve against.
_VALIDATOR = (
    "src/doppler/x/tests/validation/x/validate.py",
    'R.find("filter_on_cn0_not_margin", "FIXED", "text")\n'
    'R.find("pfa_over_delivered", "GAP", "text, #1")\n',
)

#: Every shape that is not a typed finding number, which must all pass.
_CLEAN = {
    _VALIDATOR[0]: _VALIDATOR[1],
    "native/tests/test_x.c": (
        "/* the certification's finding `filter_on_cn0_not_margin` says\n"
        " * so; findings `filter_on_cn0_not_margin` and\n"
        " * `pfa_over_delivered` both hold. A `field_name` is code. */\n"
        "static const float F0 = 0.01f; /* a frequency, not a finding */\n"
    ),
    "docs/types.md": "| float | F32 | np.float32 |\n| double | F64 | |\n",
    "src/doppler/x.py": (
        "# CF32 in, F32 out; see finding `pfa_over_delivered`\n"
    ),
}


def _seed(tmp_path: Path, files: dict[str, str]) -> None:
    for rel, text in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    subprocess.run(["git", "-C", str(tmp_path), "add", "."], check=True)


def _run(tmp_path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )


def test_keys_type_names_and_constants_pass(tmp_path: Path) -> None:
    _seed(tmp_path, _CLEAN)
    out = _run(tmp_path)
    assert out.returncode == 0, out.stdout
    assert "4 file(s), none typed" in out.stdout
    assert "against 2 key(s)" in out.stdout


@pytest.mark.parametrize(
    ("rel", "text"),
    [
        ("native/tests/test_x.c", "/* See F4. */\n"),
        ("docs/design/x.md", "records the gap as finding **F7**, and\n"),
        ("src/doppler/x/validate.py", 'ax.set_title("slope (F15)")\n'),
        ("objects/x.toml", "# a pure function of configuration (F5)\n"),
        ("native/inc/doppler/x/x_core.h", " * the report's F12 says\n"),
        ("docs/design/y.md", "measured (report §2.2, F1–F3).\n"),
    ],
)
def test_a_typed_number_fails_and_names_the_line(
    tmp_path: Path, rel: str, text: str
) -> None:
    _seed(tmp_path, {**_CLEAN, rel: "ok\n" + text})
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "cited by number" in out.stdout
    assert f"{rel}:2" in out.stdout


@pytest.mark.parametrize(
    "text",
    [
        "/* the finding `filter_on_cn0_missing` says so */\n",
        "/* findings `pfa_over_delivered` and `renamed_away` */\n",
        "/* the certification's finding\n * `renamed_away` */\n",
    ],
)
def test_a_citation_of_a_key_no_validator_records_fails(
    tmp_path: Path, text: str
) -> None:
    """The other direction: a renamed finding leaves its citation behind."""
    _seed(tmp_path, {**_CLEAN, "native/tests/test_y.c": "ok\n" + text})
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "does not exist" in out.stdout
    assert "native/tests/test_y.c:2" in out.stdout


def test_renaming_a_key_in_its_validator_fails_its_citations(
    tmp_path: Path,
) -> None:
    """The rename itself, which is the case the rule exists for."""
    renamed = _VALIDATOR[1].replace("pfa_over_delivered", "pfa_on_target")
    _seed(tmp_path, {**_CLEAN, _VALIDATOR[0]: renamed})
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "finding `pfa_over_delivered`" in out.stdout


def test_a_key_two_reports_record_fails(tmp_path: Path) -> None:
    """A shared key would let an outside citation resolve through either."""
    other = "src/doppler/y/tests/validation/y/validate.py"
    _seed(
        tmp_path,
        {**_CLEAN, other: 'R.find("pfa_over_delivered", "GAP", "y")\n'},
    )
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "names two findings" in out.stdout
    assert "`pfa_over_delivered`" in out.stdout


def test_a_validator_number_of_an_unrecorded_key_fails(
    tmp_path: Path,
) -> None:
    """number() runs only when plots are drawn, so the lint holds it."""
    text = _VALIDATOR[1] + 'ax.set_title(R.number("gone_key"))\n'
    _seed(tmp_path, {**_CLEAN, _VALIDATOR[0]: text})
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "never records" in out.stdout
    assert "`gone_key`" in out.stdout


def test_a_report_spans_its_validators_folder(tmp_path: Path) -> None:
    """A key a sibling module records is the report's own (#2752 step 3.3).

    The correlator's validate.py renders its 2-D detector flavor's part from
    detector.py beside it: validate.py cites a key detector.py records,
    and a page cites it too. Both resolve.
    """
    folder = _VALIDATOR[0].rsplit("/", 1)[0]
    files = {
        **_CLEAN,
        _VALIDATOR[0]: _VALIDATOR[1] + 'R.md(R.ref("part_two_key"))\n',
        f"{folder}/part.py": 'R.find("part_two_key", "FIXED", "text")\n',
        "docs/part.md": "as finding `part_two_key` records\n",
    }
    _seed(tmp_path, files)
    out = _run(tmp_path)
    assert out.returncode == 0, out.stdout


def test_a_sibling_modules_unrecorded_ref_fails(tmp_path: Path) -> None:
    """The folder is one report both ways: a sibling's dangling ref fails."""
    folder = _VALIDATOR[0].rsplit("/", 1)[0]
    files = {**_CLEAN, f"{folder}/part.py": 'R.md(R.ref("gone_key"))\n'}
    _seed(tmp_path, files)
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "part.py" in out.stdout and "`gone_key`" in out.stdout


def test_a_file_that_is_not_utf8_fails(tmp_path: Path) -> None:
    """An unreadable file is not a clean one: it is reported, not skipped."""
    _seed(tmp_path, _CLEAN)
    (tmp_path / "docs" / "cp.md").write_bytes("F1 \u00a7".encode("cp1252"))
    subprocess.run(["git", "-C", str(tmp_path), "add", "."], check=True)
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "docs/cp.md: UnicodeDecodeError" in out.stdout


def test_a_doc_comment_leader_citation_is_resolved(tmp_path: Path) -> None:
    """``#:`` (a Sphinx attribute comment) is a leader like ``#``."""
    _seed(
        tmp_path, {**_CLEAN, "src/doppler/y.py": "#: see finding `nope_x`\n"}
    )
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "finding `nope_x`" in out.stdout


@pytest.mark.parametrize(
    "rel",
    [
        "src/doppler/x/tests/validation/y/results.md",
        "docs/dev/contributing/validation-log.md",
        "docs/c-api/x_8h.md",
        "src/doppler/x/x.pyi",
        "CHANGELOG.md",
        "docs/dev/issues.md",
    ],
)
def test_generated_and_history_files_are_skipped_both_ways(
    tmp_path: Path, rel: str
) -> None:
    text = "still open: F3, F4; finding `long_gone`\n"
    _seed(tmp_path, {**_CLEAN, rel: text})
    out = _run(tmp_path)
    assert out.returncode == 0, out.stdout


def test_the_framework_may_type_numbers_but_not_cite_ghosts(
    tmp_path: Path,
) -> None:
    """The numbering's own framework writes `F1` to explain it; a key it
    cites is still resolved."""
    rel = "src/doppler/tests/_validation_common.py"
    _seed(tmp_path, {**_CLEAN, rel: "# F1 is the first find()\n"})
    assert _run(tmp_path).returncode == 0
    _seed(tmp_path, {**_CLEAN, rel: "# see finding `long_gone`\n"})
    assert _run(tmp_path).returncode == 1


def test_an_empty_scan_fails(tmp_path: Path) -> None:
    """A scan that read nothing is broken, not clean."""
    _seed(tmp_path, {"README": "no scanned suffix here\n"})
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "scan is broken" in out.stdout


def test_no_recorded_key_fails(tmp_path: Path) -> None:
    """With no key set, every citation check would be vacuous."""
    _seed(tmp_path, {"docs/x.md": "plain text\n"})
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "key scan is broken" in out.stdout


def test_the_gate_reads_the_one_declaration(tmp_path: Path) -> None:
    """The lint loads Report's own patterns, so the two cannot disagree."""
    text = SCRIPT.read_text(encoding="utf-8")
    assert "_finding_keys.py" in text
    assert "re.compile" not in text
