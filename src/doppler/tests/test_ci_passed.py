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
    assert "nothing untested" in r.stdout


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


# The docs-only lane: CODE_ONLY jobs may skip on an explicit code=false.
CODE_ONLY = "macos coverage"


def _run_docs(
    code: str | None, results: dict[str, str], src: str = "true"
) -> subprocess.CompletedProcess[str]:
    needs = {job: {"result": r, "outputs": {}} for job, r in results.items()}
    outputs = {"src": src}
    if code is not None:
        outputs["code"] = code
    needs["changes"] = {"result": "success", "outputs": outputs}
    env = {
        **os.environ,
        "NEEDS": json.dumps(needs),
        "SKIPPABLE": "python docs",
        "CODE_ONLY": CODE_ONLY,
    }
    return subprocess.run(
        [sys.executable, str(SCRIPT)], env=env, capture_output=True, text=True
    )


def test_docs_only_may_skip_the_code_jobs() -> None:
    r = _run_docs(
        "false",
        {"python": "success", "docs": "success", "macos": "skipped"},
    )
    assert r.returncode == 0, r.stdout
    assert "docs only" in r.stdout


def test_docs_only_may_not_skip_python() -> None:
    """Docs can break the suite: live-tree gate tests, doc fences."""
    r = _run_docs("false", {"python": "skipped", "macos": "skipped"})
    assert r.returncode == 1
    assert "python was skipped" in r.stdout


def test_a_code_job_skipped_when_code_changed_is_red() -> None:
    """The sabotage: code=true and a code-only job did not run."""
    r = _run_docs("true", {"python": "success", "macos": "skipped"})
    assert r.returncode == 1
    assert "docs cannot break it, but code changed" in r.stdout


def test_an_unset_code_is_not_docs_only() -> None:
    r = _run_docs(None, {"python": "success", "macos": "skipped"})
    assert r.returncode == 1


def test_a_code_job_failure_on_docs_only_is_still_red() -> None:
    r = _run_docs("false", {"python": "success", "macos": "failure"})
    assert r.returncode == 1


# ── the scope lane (ci_scope.py): a push without a PR, a push a PR covers ──
# PR_ONLY jobs may skip when `scope` said minimal; anything may skip when it
# said skip; neither is ever a pass for a failure, and `full` grants nothing.
PR_ONLY = "macos coverage"


def _run_scope(
    scope: str | None,
    results: dict[str, str],
    scope_result: str = "success",
    pr_only: str = PR_ONLY,
) -> subprocess.CompletedProcess[str]:
    needs = {job: {"result": r, "outputs": {}} for job, r in results.items()}
    needs["changes"] = {"result": "success", "outputs": {"src": "true"}}
    needs["scope"] = {
        "result": scope_result,
        "outputs": {} if scope is None else {"scope": scope},
    }
    env = {
        **os.environ,
        "NEEDS": json.dumps(needs),
        "SKIPPABLE": "python docs",
        "PR_ONLY": pr_only,
    }
    return subprocess.run(
        [sys.executable, str(SCRIPT)], env=env, capture_output=True, text=True
    )


def test_a_minimal_push_may_skip_the_pr_only_jobs() -> None:
    r = _run_scope(
        "minimal",
        {"python": "success", "macos": "skipped", "coverage": "skipped"},
    )
    assert r.returncode == 0, r.stdout
    assert "left to the PR run" in r.stdout
    assert "minimal set" in r.stdout


def test_a_minimal_push_may_not_skip_a_minimal_job() -> None:
    r = _run_scope("minimal", {"python": "skipped", "macos": "skipped"})
    assert r.returncode == 1
    assert "python was skipped" in r.stdout


def test_the_sabotage_a_pr_only_job_failing_is_red_on_every_scope() -> None:
    """The permission is to SKIP, never to fail: `scope` cannot green a red."""
    for scope in ("full", "minimal", "skip"):
        r = _run_scope(scope, {"python": "success", "macos": "failure"})
        assert r.returncode == 1, scope
        assert "macos ended 'failure'" in r.stdout, scope


def test_a_pr_only_job_skipped_on_a_full_run_is_red() -> None:
    """`full` is a pull request or main: the left-to-the-PR jobs are owed."""
    r = _run_scope("full", {"python": "success", "macos": "skipped"})
    assert r.returncode == 1
    assert "macos was skipped" in r.stdout


def test_a_skipped_scope_job_grants_nothing() -> None:
    """A scope that did not succeed has said nothing: no permission."""
    r = _run_scope(
        "minimal",
        {"python": "success", "macos": "skipped", "scope": "failure"},
        scope_result="failure",
    )
    assert r.returncode == 1


def test_an_unset_scope_is_not_minimal() -> None:
    r = _run_scope(None, {"python": "success", "macos": "skipped"})
    assert r.returncode == 1


def test_a_covered_push_may_skip_everything() -> None:
    r = _run_scope(
        "skip",
        {"python": "skipped", "macos": "skipped", "lint": "skipped"},
    )
    assert r.returncode == 0, r.stdout
    assert "the PR run is the gate" in r.stdout


def test_a_cancelled_job_is_red_even_when_covered() -> None:
    r = _run_scope("skip", {"python": "cancelled"})
    assert r.returncode == 1
