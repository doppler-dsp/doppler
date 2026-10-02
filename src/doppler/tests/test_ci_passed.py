"""`CI passed`'s verdict, including the bump-only fast path.

`scripts/ci_passed.py` is what the one required check runs. A version bump
alone may skip the heavy matrix, so this is where "skipped" can be green --
and the tests below are mostly the ways it must NOT be: a skip without the
classification, a skip of a gate the fast path keeps, a failure or a
cancellation on a bump.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys

from doppler.tests._repo import repo_root

SCRIPT = repo_root(__file__) / "scripts" / "ci_passed.py"
SKIPPABLE = "python coverage"


def _run(
    src: str | None, results: dict[str, str], changes: str = "success"
) -> subprocess.CompletedProcess[str]:
    needs = {job: {"result": r, "outputs": {}} for job, r in results.items()}
    needs["changes"] = {
        "result": changes,
        "outputs": {} if src is None else {"src": src},
    }
    env = {**os.environ, "NEEDS": json.dumps(needs), "SKIPPABLE": SKIPPABLE}
    return subprocess.run(
        [sys.executable, str(SCRIPT)], env=env, capture_output=True, text=True
    )


def test_all_green_passes() -> None:
    r = _run("true", {"python": "success", "lint": "success"})
    assert r.returncode == 0, r.stdout


def test_a_bump_may_skip_the_matrix() -> None:
    r = _run(
        "false",
        {"python": "skipped", "coverage": "skipped", "lint": "success"},
    )
    assert r.returncode == 0, r.stdout
    assert "version bump alone" in r.stdout


def test_a_bump_may_not_skip_a_kept_gate() -> None:
    r = _run("false", {"python": "skipped", "lint": "skipped"})
    assert r.returncode == 1
    assert "lint was skipped" in r.stdout


def test_a_failure_on_a_bump_is_still_red() -> None:
    r = _run("false", {"python": "skipped", "lint": "failure"})
    assert r.returncode == 1
    assert "lint ended 'failure'" in r.stdout


def test_a_skip_without_the_classification_is_red() -> None:
    r = _run("true", {"python": "skipped", "lint": "success"})
    assert r.returncode == 1


def test_a_skip_when_changes_did_not_run_is_red() -> None:
    r = _run(None, {"python": "skipped"}, changes="failure")
    assert r.returncode == 1
    assert "changes ended 'failure'" in r.stdout


def test_cancelled_is_red() -> None:
    r = _run("true", {"python": "cancelled"})
    assert r.returncode == 1
