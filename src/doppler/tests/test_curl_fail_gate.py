"""The curl --fail gate, exercised over seeded Dockerfiles and workflows.

`scripts/check_curl_fail.py` exists because a curl without `--fail` saves an
HTTP error page as the file it was asked for and exits 0. The CI image build
then failed at `tar` ("gzip: stdin: not in gzip format") on a URL that served
a valid tarball before and after (doppler#1738), and a `curl | bash` hands
bash the error page. Each case seeds a tree and points `--root` at it.
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
SCRIPT = REPO / "scripts" / "check_curl_fail.py"


def _run(tmp_path: Path, dockerfile: str = "", workflow: str = ""):
    d = tmp_path / "deploy" / "docker"
    d.mkdir(parents=True)
    (d / "Dockerfile.ci").write_text(dockerfile, encoding="utf-8")
    w = tmp_path / ".github" / "workflows"
    w.mkdir(parents=True)
    (w / "w.yml").write_text(workflow, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )


@pytest.mark.parametrize(
    "line",
    [
        "RUN curl -sSL https://x/get.sh | bash",  # the #1738 pipe
        "RUN a \\\n && curl -sSLo /tmp/t.tgz \\\n      https://x/t.tgz",
        "RUN curl --retry 5 -o f https://x",
    ],
)
def test_a_dockerfile_curl_without_fail_is_named(
    tmp_path: Path, line: str
) -> None:
    r = _run(tmp_path, dockerfile=f"FROM x\n{line}\n")
    assert r.returncode == 1, r.stdout
    assert "deploy/docker/Dockerfile.ci:2: curl" in r.stdout


@pytest.mark.parametrize(
    "line",
    [
        "RUN curl -fsSL --retry 5 -o f https://x",
        "RUN curl -LsSf https://x/install.sh -o i.sh",  # f inside a cluster
        "RUN curl --fail -o f https://x",
        "RUN curl \\\n      --fail-with-body -o f https://x",
        # A package name in an install list is not an invocation.
        "RUN apt-get install -y ca-certificates curl git",
        "# a comment that mentions curl -sSL x | bash",
    ],
)
def test_a_failing_curl_or_a_non_invocation_passes(
    tmp_path: Path, line: str
) -> None:
    r = _run(tmp_path, dockerfile=f"FROM x\n{line}\n")
    assert r.returncode == 0, r.stdout


def test_a_workflow_curl_without_fail_is_named(tmp_path: Path) -> None:
    wf = (
        "jobs:\n  a:\n    steps:\n      - run: |\n"
        "          if curl -s x; then true; fi\n"
    )
    r = _run(tmp_path, workflow=wf)
    assert r.returncode == 1, r.stdout
    assert ".github/workflows/w.yml:5: curl -s x" in r.stdout


def test_the_flags_of_the_next_command_do_not_count(tmp_path: Path) -> None:
    """`curl -s x | grep -f pats` must not borrow grep's -f."""
    r = _run(tmp_path, dockerfile="RUN curl -s x | grep -f pats\n")
    assert r.returncode == 1, r.stdout


def test_the_live_tree_passes() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout


def test_the_ci_image_extra_script_is_in_scope(tmp_path: Path) -> None:
    """docker/ci-extra.sh runs in the shared CI image's build (gh-885)."""
    d = tmp_path / "docker"
    d.mkdir()
    (d / "ci-extra.sh").write_text("curl -sSL -o t https://x\n", "utf-8")
    r = _run(tmp_path)
    assert r.returncode == 1, r.stdout
    assert "docker/ci-extra.sh:1: curl" in r.stdout
