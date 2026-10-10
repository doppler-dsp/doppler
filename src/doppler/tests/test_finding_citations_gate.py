"""The finding-citation gate, exercised over a seeded tree.

`scripts/check_finding_citations.py` holds #2059's rule outside the
reports: a validation finding is cited by its key or its claim, never by
its number, because a report numbers findings by position and a number
re-points the moment one before it is dropped. Inside a report the render
step refuses a typed number (`test_validation_report.py`); this gate is
the half for every file with no render step.

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

#: Every shape that is not a typed finding number, which must all pass.
_CLEAN = {
    "native/tests/test_x.c": (
        "/* the certification's `filter_on_cn0_not_margin` says so */\n"
        "static const float F0 = 0.01f; /* a frequency, not a finding */\n"
    ),
    "docs/types.md": "| float | F32 | np.float32 |\n| double | F64 | |\n",
    "src/doppler/x.py": "# CF32 in, F32 out; see `pfa_over_delivered`\n",
}


def _seed(tmp_path: Path, files: dict[str, str]) -> None:
    for rel, text in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
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
    assert "3 file(s), none typed" in out.stdout


@pytest.mark.parametrize(
    ("rel", "text"),
    [
        ("native/tests/test_x.c", "/* See F2. */\n"),
        ("docs/design/x.md", "records the gap as finding **F7**, and\n"),
        ("src/doppler/x/validate.py", 'ax.set_title("slope (F15)")\n'),
        ("objects/x.toml", "# a pure function of configuration (F5)\n"),
        ("native/inc/doppler/x/x_core.h", " * the report's F12 says\n"),
    ],
)
def test_a_typed_number_fails_and_names_the_line(
    tmp_path: Path, rel: str, text: str
) -> None:
    _seed(tmp_path, {**_CLEAN, rel: "ok\n" + text})
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert f"{rel}:2" in out.stdout


@pytest.mark.parametrize(
    "rel",
    [
        "src/doppler/x/tests/validation/y/results.md",
        "docs/dev/contributing/validation-log.md",
        "docs/c-api/x_8h.md",
        "src/doppler/x/x.pyi",
        "CHANGELOG.md",
        "docs/dev/issues.md",
        "src/doppler/tests/_validation_common.py",
    ],
)
def test_generated_history_and_framework_files_are_skipped(
    tmp_path: Path, rel: str
) -> None:
    _seed(tmp_path, {**_CLEAN, rel: "still open: F3, F4\n"})
    out = _run(tmp_path)
    assert out.returncode == 0, out.stdout


def test_an_empty_scan_fails(tmp_path: Path) -> None:
    """A scan that read nothing is broken, not clean."""
    _seed(tmp_path, {"README": "no scanned suffix here\n"})
    out = _run(tmp_path)
    assert out.returncode == 1, out.stdout
    assert "scan is broken" in out.stdout
