"""The design-page gate, exercised over seeded pages.

`scripts/check_design_pages.py` guards a convention this repository wrote
down, applied once by hand, and then lost. `docs/design/async-dsss-receiver.md`
was split on 2026-09-07 at **3686 lines**: the dated record moved to a
companion `-measurements.md` page keeping the same section numbers, because
issues and harnesses cite `§12.17`. The room refilled — the design page was
back to 2270 lines and 17% play-by-play before this gate existed.

A gate proven only against a tree that happens to pass is a gate nobody has
seen fail. So the script takes explicit page arguments and this file drives it
over pages written here: each of the three rules must be caught, the companion
record page must be exempt, a fenced example must not be a finding, and an
empty scan must fail rather than pass vacuously.
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
SCRIPT = REPO / "scripts" / "check_design_pages.py"

CLEAN = """\
# Widget — the fixed-point input budget

This page is that budget.

## The budget has two terms

They are not the same term.

## What this is not

Not a filter design guide.
"""


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


def _page(tmp_path: Path, name: str, body: str) -> str:
    p = tmp_path / name
    p.write_text(body, encoding="utf-8")
    return str(p)


def test_a_clean_page_passes(tmp_path: Path) -> None:
    proc = _run(_page(tmp_path, "widget.md", CLEAN))
    assert proc.returncode == 0, proc.stdout


@pytest.mark.parametrize(
    ("rule", "line"),
    [
        ("dated-heading", "## 12.28 What it did (2026-09-10)"),
        ("status-preamble", "**Status:** built through step 6"),
        ("status-preamble", "*Phase 1 of adding an algorithm.*"),
        ("status-preamble", "*Nothing below is implemented.*"),
        ("record-heading", "## 12. The work that answered it"),
        ("record-heading", "## The record — resolved and open"),
        ("record-heading", "## 13. What this page does not settle"),
        ("record-heading", "## 6. Open questions the inventory raises"),
    ],
    ids=lambda v: v.replace(" ", "-")[:28],
)
def test_each_rule_is_caught(tmp_path: Path, rule: str, line: str) -> None:
    """Every pattern the convention forbids, one seeded page each."""
    proc = _run(_page(tmp_path, "widget.md", CLEAN + "\n" + line + "\n"))
    assert proc.returncode == 1, proc.stdout
    assert rule in proc.stdout
    assert "narrates its own construction" in proc.stdout


def test_the_companion_record_page_is_exempt(tmp_path: Path) -> None:
    """`-measurements.md` is the home the convention creates for this.

    Exempted by NAME rather than by a list, so a new companion page is
    covered the moment it exists.
    """
    body = (
        CLEAN
        + "\n## 12.1 What was measured (2026-09-02) — the floor\n"
        + "\n**Status:** a record.\n"
    )
    proc = _run(_page(tmp_path, "widget-measurements.md", body))
    assert proc.returncode == 0, proc.stdout


def test_a_fenced_example_is_not_a_finding(tmp_path: Path) -> None:
    """Docs about docs show the forbidden shapes as examples."""
    body = CLEAN + "\n```text\n## 12.28 What was measured (2026-09-10)\n```\n"
    proc = _run(_page(tmp_path, "widget.md", body))
    assert proc.returncode == 0, proc.stdout


def test_status_mid_sentence_is_not_a_finding(tmp_path: Path) -> None:
    """The rule is a preamble, not the word. Prose about state is fine."""
    body = CLEAN + "\nThe lock **Status:** field is read once per block.\n"
    proc = _run(_page(tmp_path, "widget.md", body))
    assert proc.returncode == 0, proc.stdout


def test_an_empty_scan_fails(tmp_path: Path) -> None:
    """An empty result set is not a pass.

    The same trap the glibc and tarball gates were both caught by: a scan
    that matches nothing has not run, so it has not passed.
    """
    proc = _run(str(tmp_path / "no-such-page.md"))
    assert proc.returncode == 1, proc.stdout


def test_the_repository_matches_its_baseline() -> None:
    """The gate over the real tree, which is what CI runs."""
    proc = _run()
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "may only shrink" in proc.stdout
