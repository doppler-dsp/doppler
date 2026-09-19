"""The `CI passed` aggregator gate, exercised over seeded workflows.

`scripts/check_ci_aggregator.py` exists because `protect-main` was relaxed on
2026-09-14 to require a single status check, `CI passed`. From then on a job
gates a merge only through that job's `needs` list -- a hand-kept YAML list
that nothing read. Each case below seeds the shape that would make a red job
stop blocking, so it fails if the gate cannot see it.
"""

from __future__ import annotations

import subprocess
import sys
import textwrap
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_ci_aggregator.py"


def _check(tmp_path: Path, body: str):
    f = tmp_path / "ci.yml"
    f.write_text(textwrap.dedent(body), encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(f)], capture_output=True, text=True
    )


_SOUND = """
    jobs:
      build:
        runs-on: ubuntu-latest
      test:
        runs-on: ubuntu-latest
      ci-passed:
        name: CI passed
        needs: [build, test]
        if: always()
        runs-on: ubuntu-latest
"""


def test_a_sound_aggregator_passes(tmp_path: Path) -> None:
    r = _check(tmp_path, _SOUND)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "all 2 job(s)" in r.stdout


def test_a_job_missing_from_needs_is_named(tmp_path: Path) -> None:
    """The failure this gate exists for: a new job nobody added to needs."""
    r = _check(
        tmp_path,
        _SOUND.replace(
            "      ci-passed:",
            "      lint:\n        runs-on: ubuntu-latest\n      ci-passed:",
        ),
    )
    assert r.returncode == 1
    assert "`lint` is not in `ci-passed`'s needs" in r.stdout


def test_a_need_that_names_no_job_is_named(tmp_path: Path) -> None:
    r = _check(
        tmp_path, _SOUND.replace("[build, test]", "[build, test, gone]")
    )
    assert r.returncode == 1
    assert "needs `gone`, which is not a job here" in r.stdout


def test_a_renamed_aggregator_is_refused(tmp_path: Path) -> None:
    """The ruleset requires the check by its display NAME."""
    r = _check(tmp_path, _SOUND.replace("name: CI passed", "name: CI green"))
    assert r.returncode == 1
    assert "requires the check 'CI passed'" in r.stdout


def test_an_aggregator_without_always_is_refused(tmp_path: Path) -> None:
    """Without `if: always()` a failed need SKIPS it; a skip does not block."""
    r = _check(tmp_path, _SOUND.replace("        if: always()\n", ""))
    assert r.returncode == 1
    assert "must run `if: always()`" in r.stdout


def test_no_aggregator_is_refused(tmp_path: Path) -> None:
    body = _SOUND.split("      ci-passed:")[0]
    r = _check(tmp_path, body)
    assert r.returncode == 1
    assert "no `ci-passed` job" in r.stdout


def test_the_live_ci_workflow_passes() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True, cwd=REPO
    )
    assert r.returncode == 0, r.stdout + r.stderr


_FAST = """
    jobs:
      changes:
        runs-on: ubuntu-latest
      lint:
        runs-on: ubuntu-latest
      matrix:
        needs: {needs}
        if: needs.changes.outputs.src == 'true'
        runs-on: ubuntu-latest
      ci-passed:
        name: CI passed
        needs: [changes, lint, matrix]
        if: always()
        runs-on: ubuntu-latest
        steps:
          - env:
              SKIPPABLE: {skippable}
            run: python3 scripts/ci_passed.py
"""


def test_a_sound_fast_path_passes(tmp_path: Path) -> None:
    r = _check(tmp_path, _FAST.format(needs="changes", skippable="matrix"))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_gated_job_must_need_changes(tmp_path: Path) -> None:
    # Without it the condition reads an empty output: skipped on every diff.
    r = _check(tmp_path, _FAST.format(needs="lint", skippable="matrix"))
    assert r.returncode == 1
    assert "does not need it" in r.stdout


def test_a_gated_job_missing_from_skippable_is_caught(
    tmp_path: Path,
) -> None:
    r = _check(tmp_path, _FAST.format(needs="changes", skippable="other"))
    assert r.returncode == 1
    assert "not in `ci-passed`'s SKIPPABLE" in r.stdout


def test_skippable_may_not_name_an_ungated_job(tmp_path: Path) -> None:
    # The dangerous direction: permission to skip a gate that never skips.
    r = _check(
        tmp_path, _FAST.format(needs="changes", skippable="matrix lint")
    )
    assert r.returncode == 1
    assert "lists `lint`, which is not gated" in r.stdout
