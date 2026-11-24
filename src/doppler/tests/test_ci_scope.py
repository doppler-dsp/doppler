"""`ci_scope.py`: how much of CI a run owes (full, minimal or skip).

ci.yml runs on every push, so this is the one rule that says what a push
without a pull request runs and when a push is already covered by its PR.
The cases are the ones a wrong guess costs: a PR's head sha that is NOT the
pushed one (a force-push racing the PR event), a fork's same-named branch,
`main` and `develop` (always full), and a tag-less, PR-less branch.
"""

from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "ci_scope.py"
_spec = importlib.util.spec_from_file_location("ci_scope", SCRIPT)
assert _spec is not None and _spec.loader is not None
ci_scope = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ci_scope)

BRANCH = "refs/heads/fix/x"


@pytest.mark.parametrize(
    "event", ["pull_request", "schedule", "workflow_dispatch"]
)
def test_every_event_but_a_push_is_full(event: str) -> None:
    assert ci_scope.scope(event, "refs/pull/7/merge", "a", ("a",)) == "full"


@pytest.mark.parametrize("ref", ["refs/heads/main", "refs/heads/develop"])
def test_a_push_to_a_long_lived_branch_is_full(ref: str) -> None:
    """Even when a PR's head is this very sha: main keeps today's rule."""
    assert ci_scope.scope("push", ref, "a", ("a",)) == "full"


def test_a_push_without_a_pr_is_minimal() -> None:
    assert ci_scope.scope("push", BRANCH, "a", ()) == "minimal"


def test_a_push_at_a_prs_head_is_skip() -> None:
    assert ci_scope.scope("push", BRANCH, "a", ("a",)) == "skip"


def test_a_push_past_a_prs_head_is_minimal() -> None:
    """The PR run tests the head the PR event saw, not this newer sha."""
    assert ci_scope.scope("push", BRANCH, "b", ("a",)) == "minimal"


def test_main_writes_the_output(tmp_path: Path) -> None:
    """End to end for the no-PR path that needs no `gh`: a push to main."""
    out = tmp_path / "out"
    env = {
        **os.environ,
        "EVENT": "push",
        "REF": "refs/heads/main",
        "SHA": "0123456789abcdef",
        "BRANCH": "main",
        "REPO": "o/r",
        "GITHUB_OUTPUT": str(out),
    }
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], env=env, capture_output=True, text=True
    )
    assert r.returncode == 0, r.stderr
    assert out.read_text(encoding="utf-8") == (
        'scope=full\nlinux_os=["ubuntu-22.04","ubuntu-24.04"]\n'
    )


@pytest.mark.parametrize(
    ("result", "want"),
    [
        ("minimal", ["ubuntu-24.04"]),
        ("full", ["ubuntu-22.04", "ubuntu-24.04"]),
        ("skip", ["ubuntu-22.04", "ubuntu-24.04"]),
    ],
)
def test_the_linux_os_list_is_the_matrix_input(
    result: str, want: list[str]
) -> None:
    """The JSON the matrix reads with fromJSON. A minimal push drops the
    22.04 leg; any unknown scope builds both, failing toward more."""
    import json

    assert json.loads(ci_scope.linux_os(result)) == want


def test_an_unknown_scope_builds_both() -> None:
    import json

    assert json.loads(ci_scope.linux_os("garbage")) == [
        "ubuntu-22.04",
        "ubuntu-24.04",
    ]
