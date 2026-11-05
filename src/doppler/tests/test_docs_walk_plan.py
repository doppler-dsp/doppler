"""The docs walk plans its commands from the pages, and fails on an empty one.

`tests/install/docs-walk.sh plan` prints what each stage would run, extracted
from the page that tells the reader to run it. It is the walk's ONE
declaration of what to extract, so this test calls it rather than restating
its patterns: change an extraction and these see it. No container and no
network (doppler#2857).
"""

from __future__ import annotations

import os
import shutil
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
INSTALL = REPO / "tests" / "install"
WALK = INSTALL / "docs-walk.sh"
pytestmark = pytest.mark.skipif(
    shutil.which("bash") is None, reason="needs bash"
)


def _plan(stage: str = "", env: dict[str, str] | None = None, repo=REPO):
    return subprocess.run(
        ["bash", str(WALK), "plan", *([stage] if stage else [])],
        capture_output=True,
        text=True,
        env={
            **os.environ,
            "WALK_REPO": str(repo),
            "WALK_VERSION": "1.2.3",
            **(env or {}),
        },
    )


@pytest.mark.parametrize(
    "script", ["docs-walk.sh", "docs-walk-build.sh", "docs-walk-host.sh"]
)
def test_the_walk_scripts_parse(script: str) -> None:
    r = subprocess.run(
        ["bash", "-n", str(INSTALL / script)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stderr


@pytest.mark.parametrize(
    ("stage", "expected"),
    [
        ("pip", 'pip install "doppler-dsp[examples]"'),
        ("pip", "python -m doppler.examples.delay_doppler_bank_demo"),
        ("pip", "wfmgen --from-file scenario.json"),
        ("tarball", "jbx get-doppler"),
        ("tarball", 'PREFIX="$HOME/.local/doppler"'),
        ("clone", "git clone --branch v"),
        ("clone", "make build"),
        ("deb", "consumer-walk.sh"),
        ("docker", "docker run --rm ghcr.io/doppler-dsp/doppler:"),
    ],
)
def test_each_stage_yields_the_pages_command(
    stage: str, expected: str
) -> None:
    r = _plan(stage)
    assert r.returncode == 0, r.stderr
    assert expected in r.stdout


def test_the_candidate_face_substitutes_once_per_page_command() -> None:
    r = _plan("pip", {"WALK_MODE": "candidate", "WALK_WHEEL": "/art/x.whl"})
    assert 'pip install "/art/x.whl[examples]"' in r.stdout
    assert "doppler-dsp[examples]" not in r.stdout
    c = _plan("clone", {"WALK_MODE": "candidate"})
    assert "--branch" not in c.stdout.split("git clone")[1].split("\n")[0]


def test_a_page_that_stops_yielding_a_command_fails_the_plan(
    tmp_path: Path,
) -> None:
    """An empty extraction must be an error, not a stage that runs nothing."""
    shutil.copytree(REPO / "docs", tmp_path / "docs")
    page = tmp_path / "docs" / "install" / "docker.md"
    page.write_text(
        page.read_text(encoding="utf-8").replace("doppler --help", "x"),
        encoding="utf-8",
    )
    r = _plan("docker", repo=tmp_path)
    assert r.returncode != 0
    assert "extracted nothing" in r.stderr
