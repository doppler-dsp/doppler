"""The docs walk plans its commands from the pages, and fails on an empty one.

`tests/install/docs-walk.sh plan` prints what each stage would run, extracted
from the page that tells the reader to run it. It is the walk's ONE
declaration of what to extract, so this test calls it rather than restating
its patterns: change an extraction and these see it. No container and no
network (doppler#2857).
"""

from __future__ import annotations

import fnmatch
import os
import re
import shutil
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._platform import posix_only
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
INSTALL = REPO / "tests" / "install"
WALK = INSTALL / "docs-walk.sh"
pytestmark = posix_only(
    "runs bash (System32's bash is the WSL launcher on a Windows runner)"
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
        ("pip", "python3 -m venv .venv"),
        ("pip", "sudo apt install python3-venv"),
        ("deb", "sudo apt install ./libdoppler-dsp0"),
        ("deb", "tar -xz --strip-components=2"),
        ("deb", "make run"),
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


def test_the_candidate_workflow_triggers_on_every_file_the_walk_reads() -> (
    None
):
    """A page the walk extracts from can break it, so a PR that edits one must
    run the candidate face. The list is the script's own (`pages`), not a
    second copy that drifts."""
    read = subprocess.run(
        ["bash", str(WALK), "pages"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.split()
    assert len(read) >= 8, read
    wf = (REPO / ".github" / "workflows" / "docs-walk.yml").read_text(
        encoding="utf-8"
    )
    block = wf.split("    paths:\n", 1)[1].split("  workflow_dispatch:", 1)[0]
    filters = re.findall(r"^      - (\S+)$", block, re.M)
    missing = [
        f for f in read if not any(fnmatch.fnmatch(f, g) for g in filters)
    ]
    assert not missing, f"docs-walk.yml's paths miss: {missing}"


def test_the_deb_plan_is_the_debian_block_not_the_rpm_one() -> None:
    r = _plan("deb")
    assert r.returncode == 0, r.stderr
    assert "dnf" not in r.stdout
    assert "sudo apt install ./libdoppler-dsp0" in r.stdout
