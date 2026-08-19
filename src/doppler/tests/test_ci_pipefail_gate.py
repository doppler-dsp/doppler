"""The CI pipefail gate, exercised over seeded workflow YAML.

`scripts/check_workflow_pipelines.py` guards a defect that has already cost
this repository a day: `run: make coverage | tee coverage.txt` under the
default `bash -e` shell reports *tee's* exit code, so the step went green
over a `make coverage` that had died, and the failure only surfaced one step
later as the patch gate opening a `coverage.lcov` nothing had written.

A gate proven only by running it against a tree that happens to pass is a
gate nobody has seen fail — the same "cannot fail independently" shape the
gate itself exists to catch. So the script takes explicit file arguments and
this file drives it over YAML written here: a known-bad step must be caught,
and each of the three ways to declare pipefail must clear it.

The quoting cases are not decoration either. A `|` inside a `jq` filter or an
`awk` program is data, not a pipeline, and a scanner that flags them makes
the gate unusable; a `|` inside `$( )` *is* a pipeline even when the
substitution sits inside double quotes, and a scanner that misses that reads
release.yml's `gh api ... | head -n1` as clean — which the first draft did.
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
SCRIPT = REPO / "scripts" / "check_workflow_pipelines.py"


def _check(tmp_path: Path, run: str, **step: str) -> int:
    """Write a one-step workflow around `run` and return the gate's code."""
    extra = "".join(f"        {k}: {v}\n" for k, v in step.items())
    body = "\n".join(f"          {ln}" for ln in run.splitlines())
    wf = tmp_path / "seeded.yml"
    wf.write_text(
        "name: seeded\n"
        "on: [push]\n"
        "jobs:\n"
        "  job:\n"
        "    steps:\n"
        "      - name: step\n"
        f"{extra}"
        "        run: |\n"
        f"{body}\n",
        encoding="utf-8",
    )
    proc = subprocess.run(
        [sys.executable, str(SCRIPT), str(wf)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    return proc.returncode


def test_bare_pipeline_is_caught(tmp_path: Path) -> None:
    """The exact line that shipped: a `make` piped into `tee`."""
    assert _check(tmp_path, "make coverage | tee coverage.txt") == 1


def test_pipeline_inside_command_substitution_is_caught(
    tmp_path: Path,
) -> None:
    """`$( )` restarts quoting; the pipe inside it still discards status."""
    run = 'line="$(gh api "repos/$REPO/x" --jq \'.a[] | .b\' | head -n1)"'
    assert _check(tmp_path, run) == 1


@pytest.mark.parametrize(
    ("run", "step"),
    [
        ("make coverage | tee coverage.txt", {"shell": "bash"}),
        (
            "make coverage | tee coverage.txt",
            {"shell": "bash --noprofile -eo pipefail {0}"},
        ),
        ("set -o pipefail\nmake coverage | tee coverage.txt", {}),
    ],
    ids=["shell-bash", "explicit-shell-string", "set-o-pipefail"],
)
def test_declared_pipefail_passes(
    tmp_path: Path, run: str, step: dict[str, str]
) -> None:
    """All three ways of putting pipefail in effect clear the gate."""
    assert _check(tmp_path, run, **step) == 0


@pytest.mark.parametrize(
    "run",
    [
        "gh api x --jq '.runs[] | .name'",
        "awk '{print $1 \"|\" $2}' f.txt",
        "make test || echo failed",
        "# a commented | pipe\nmake test",
        'echo "a|b"',
    ],
    ids=["jq-filter", "awk-program", "or-list", "comment", "quoted-data"],
)
def test_quoted_and_non_pipe_bars_are_not_pipelines(
    tmp_path: Path, run: str
) -> None:
    """A `|` that pipes nothing must not be reported."""
    assert _check(tmp_path, run) == 0


def test_the_tree_itself_holds() -> None:
    """Every workflow in the repo passes — this is what `make lint` runs."""
    proc = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr
