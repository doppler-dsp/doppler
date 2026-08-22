"""The workflow shell-syntax gate, exercised over seeded workflows.

`scripts/check_workflow_syntax.py` exists because of the v0.43.0 release. The
C-library step passed its script to a container as a double-quoted argument,
and a COMMENT inside it carried an unbalanced `"`:

    # than assumed: without the line `git archive` prints "detected
    # dubious ownership" and writes nothing, and with it the export

That closed the argument early, so every line after it was parsed by the
RUNNER instead of the container. `make package-starter-tarball` therefore ran
as the runner user against a `build/` that cmake had just created as root
inside the container, and the release died on

    mkdir: cannot create directory 'build/starter-pkg': Permission denied

— a permissions error with no permissions bug behind it, on the job that
builds the release tarballs, after the wheels had already reached PyPI.
`bash -n` reports it in under a millisecond.

The fixture is that block reduced to the two lines that carry the defect --
a comment whose `"` is unbalanced inside a double-quoted container argument --
so it cannot drift into testing something easier than what happened.
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
SCRIPT = REPO / "scripts" / "check_workflow_syntax.py"


def _check(tmp_path: Path, body: str, name: str = "wf.yml"):
    f = tmp_path / name
    f.write_text(textwrap.dedent(body), encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(f)], capture_output=True, text=True
    )


def test_the_real_v0_43_0_block_is_rejected(tmp_path: Path) -> None:
    """The exact shape that broke the release, verbatim."""
    r = _check(
        tmp_path,
        """
        jobs:
          build-c-linux:
            steps:
              - name: Build + tar the C library in the manylinux container
                run: |
                  docker run --rm -v "$PWD:/project" -w /project img \\
                    bash -euxc "
                      make package-c-tarball VERSION=1.0.0
                      # without the line `git archive` prints "detected
                      # dubious ownership" and writes nothing
                      make package-starter-tarball VERSION=1.0.0
                    "
        """,
    )
    assert r.returncode == 1, r.stdout
    assert "unexpected EOF" in r.stdout
    # naming the job and step is the difference between a usable report and
    # "something in your workflows is wrong"
    assert "build-c-linux" in r.stdout
    assert "manylinux container" in r.stdout


def test_the_heredoc_fix_passes(tmp_path: Path) -> None:
    """A quoted heredoc makes the same comments inert."""
    r = _check(
        tmp_path,
        """
        jobs:
          build-c-linux:
            steps:
              - name: Build + tar the C library in the manylinux container
                run: |
                  docker run --rm -i -v "$PWD:/project" -w /project img \\
                    bash -euxs <<'IN_CONTAINER'
                      make package-c-tarball VERSION=1.0.0
                      # without the line `git archive` prints "detected
                      # dubious ownership" and writes nothing
                      make package-starter-tarball VERSION=1.0.0
                  IN_CONTAINER
        """,
    )
    assert r.returncode == 0, r.stdout


def test_a_gha_expression_is_not_a_syntax_error(tmp_path: Path) -> None:
    """`${{ }}` is substituted before the shell runs, so it must not trip."""
    r = _check(
        tmp_path,
        """
        jobs:
          j:
            steps:
              - run: make release VERSION=${{ needs.v.outputs.version }}
              - run: echo "${{ matrix.arch.tag }}"
        """,
    )
    assert r.returncode == 0, r.stdout


def test_a_non_bash_shell_is_skipped(tmp_path: Path) -> None:
    """Python is not shell; guessing at it would make the gate unusable."""
    r = _check(
        tmp_path,
        """
        jobs:
          j:
            steps:
              - shell: python
                run: |
                  d = {"unbalanced": 'quotes are fine here'}
                  print(d)
        """,
    )
    assert r.returncode == 0, r.stdout


def test_the_live_workflows_pass(tmp_path: Path) -> None:
    """The gate's own tree must be clean, or it is reporting nothing."""
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True, cwd=REPO
    )
    assert r.returncode == 0, r.stdout + r.stderr
