"""The commands `make consumer-walk` runs are extractable from the pages.

The walk itself needs a container and the network, so it runs after a release.
What can go wrong at REVIEW time is quieter: a page edit that makes
`tests/install/fence.sh` find nothing, so a step would run an empty script and
pass, or find a block bash cannot parse. This runs the same extraction on the
PR tree and holds each block non-empty and `bash -n` clean.
"""

from __future__ import annotations

import subprocess

import pytest

from doppler.tests._platform import posix_only
from doppler.tests._repo import repo_root

# `bash` is System32's WSL launcher on a Windows runner (searched ahead of
# PATH, and exits 1 with no distribution installed), so these cases cannot run
# there. The walk itself runs in a Linux container after a release, and the
# extraction is the same awk on every Linux leg of CI.
pytestmark = posix_only("execs bash; the walk runs on Linux release runners")

REPO = repo_root(__file__)
FENCE = str(REPO / "tests" / "install" / "fence.sh")
# (page, heading regex, a line the block must contain): what the walk uses.
STEPS = [
    ("docs/install/c.md", "^Debian / Ubuntu:", "libdoppler-dsp-dev_"),
    ("docs/install/c.md", "^### Build something against it", "apt install"),
    ("docs/example-projects/consumer.md", "^## Get the code", "tar -xz"),
]


@pytest.mark.parametrize(("page", "heading", "needle"), STEPS)
def test_a_walk_step_extracts_and_parses(
    page: str, heading: str, needle: str
) -> None:
    r = subprocess.run(
        ["bash", FENCE, str(REPO / page), heading, needle],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, f"fence.sh exited {r.returncode}: {r.stderr}"
    assert r.stdout.strip(), f"{page}: nothing extracted for {needle!r}"
    assert needle in r.stdout
    syntax = subprocess.run(
        ["bash", "-n"], input=r.stdout, capture_output=True, text=True
    )
    assert syntax.returncode == 0, syntax.stderr
    # one command per step is what the walk's `cat` and a reader expect
    assert "sudo apt install" in r.stdout or "curl" in r.stdout


def test_the_apt_step_is_the_whole_block_not_one_line_of_it() -> None:
    r = subprocess.run(
        ["bash", FENCE, str(REPO / STEPS[1][0]), STEPS[1][1], "apt install"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, f"fence.sh exited {r.returncode}: {r.stderr}"
    assert "dnf" not in r.stdout
    assert r.stdout.strip().startswith("sudo apt install")
