"""`CONTAINER_USER` runs a container as the caller, under docker and podman.

Every Makefile container that writes into the checkout must run as the
caller, or what it writes is not the caller's. Under docker that is
``-u uid:gid``. Under rootless podman, which is often what ``docker`` is, the
same flag maps to a subordinate uid on the host, and the container cannot
write the mount at all: ``gen-c-api``'s doxygen fallback then fails as a
missing ``index.xml`` with no hint of why (#2933).

The decision lives in one declaration, ``CONTAINER_USER``. These tests drive
it with a stub ``docker`` for each engine, so they need neither installed,
and require that the two sites that write through a mount use it.
"""

from __future__ import annotations

import os
import re
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
MAKEFILE = REPO / "Makefile"

# The containers these recipes start, and the uid they run as, are POSIX: no
# Windows leg runs one, and os.getuid does not exist there.
posix_only = pytest.mark.skipif(
    os.name != "posix", reason="container recipes and uids are POSIX-only"
)


def _container_user(tmp_path: Path, version_line: str) -> str:
    """What CONTAINER_USER expands to when `docker --version` prints this."""
    stub = tmp_path / "bin"
    stub.mkdir()
    docker = stub / "docker"
    docker.write_text(f"#!/bin/sh\necho '{version_line}'\n")
    docker.chmod(0o755)
    env = {**os.environ, "PATH": f"{stub}{os.pathsep}{os.environ['PATH']}"}
    r = subprocess.run(
        [
            "make",
            "-s",
            "--no-print-directory",
            "--eval",
            "_cu_probe: ; @echo $(CONTAINER_USER)",
            "_cu_probe",
        ],
        capture_output=True,
        text=True,
        cwd=REPO,
        env=env,
        check=True,
    )
    return r.stdout.strip().splitlines()[-1]


@pytest.mark.parametrize(
    ("version_line", "want"),
    [
        ("podman version 5.8.9", "--userns=keep-id"),
        ("Docker version 27.3.1, build ce12230", "-u {uid}:{gid}"),
    ],
    ids=["podman", "docker"],
)
@posix_only
def test_container_runs_as_the_caller(
    tmp_path: Path, version_line: str, want: str
) -> None:
    want = want.format(uid=os.getuid(), gid=os.getgid())
    assert _container_user(tmp_path, version_line) == want


def test_every_writing_container_uses_container_user() -> None:
    """The uid is spelled once, and both writing sites go through it.

    A second ``id -u`` is a site deciding for itself, which is how the
    gen-c-api fallback kept ``-u`` after the checkout mount needed more.
    """
    text = MAKEFILE.read_text(encoding="utf-8")
    uid_lines = [
        ln
        for ln in text.splitlines()
        if "id -u" in ln and not ln.startswith("#")
    ]
    assert len(uid_lines) == 1, uid_lines
    checkout = r"^CONTAINER_CHECKOUT = \$\(CONTAINER_USER\)"
    assert re.search(checkout, text, re.M)
    shim = re.search(
        r"exec docker run --rm -i %s .*\n.*\"\$\(CONTAINER_USER\)\"", text
    )
    assert shim, (
        "gen-c-api's doxygen shim must take its user from CONTAINER_USER"
    )
