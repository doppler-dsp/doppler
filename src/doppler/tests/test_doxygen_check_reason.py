"""`make doxygen-check` says why it could not reach CI's doxygen (#2362).

When the local doxygen is not CI's pinned version, the check runs CI's
doxygen inside the CI image through docker. It used to report every
failure of that probe as "the CI image ships doxygen , not 1.9.8 ... CI has
moved", including a machine with no docker at all, and told the reader to
bump a pin that was right. Only an image that RAN and reported another
version means CI moved; a missing docker and an image that would not run are
each named for what they are.

Each case runs the real target with PATH limited to a temp dir holding
`cut` and, where the case needs one, a fake `docker`, so the result does not
depend on what the machine has installed. `DOXYGEN_VERSION` is set to a
version no doxygen reports, so the local doxygen (CI has the real one)
never matches and the target always takes the image path. The passing
case stands in for the image too: its fake docker reports the pin and then
"runs" the gate, which is the path a machine with real docker takes.
"""

from __future__ import annotations

import os
import shutil
import stat
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._platform import posix_only
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

# The case narrows PATH to a temp dir holding `cut` and a `#!/bin/sh` fake
# docker. On Windows that drops sh.exe, so mingw make falls back to cmd.exe
# and the recipe cannot run at all; and doxygen-check runs on the Linux leg
# only, so there is no Windows behaviour here to test.
pytestmark = posix_only(
    "narrows PATH to a fake #!/bin/sh docker, which drops the sh.exe mingw "
    "make needs; doxygen-check runs on Linux only"
)

REPO = repo_root(__file__)
PIN = "0.0.0-test"
MAKE = shutil.which("make") or "make"


def _run(tmp_path: Path, docker: str | None) -> subprocess.CompletedProcess:
    """doxygen-check with only `cut` and, if given, a fake docker on PATH."""
    bindir = tmp_path / "bin"
    bindir.mkdir()
    cut = shutil.which("cut")
    assert cut, "cut is needed to run the target at all"
    (bindir / "cut").symlink_to(cut)
    if docker is not None:
        fake = bindir / "docker"
        fake.write_text("#!/bin/sh\n" + docker, encoding="utf-8")
        fake.chmod(fake.stat().st_mode | stat.S_IXUSR)
    return subprocess.run(
        [
            MAKE,
            "-s",
            "-C",
            str(REPO),
            "doxygen-check",
            f"DOXYGEN_VERSION={PIN}",
        ],
        capture_output=True,
        text=True,
        env={**os.environ, "PATH": str(bindir), "MAKEFLAGS": ""},
    )


def test_a_missing_docker_is_named_not_blamed_on_ci(tmp_path: Path) -> None:
    r = _run(tmp_path, docker=None)
    out = r.stdout + r.stderr
    assert r.returncode != 0, out
    assert "docker is" in out and "not installed" in out, out
    assert "CI has moved" not in out, out


def test_an_image_that_will_not_run_is_named(tmp_path: Path) -> None:
    r = _run(
        tmp_path,
        docker='echo "Cannot connect to the Docker daemon" >&2\nexit 125\n',
    )
    out = r.stdout + r.stderr
    assert r.returncode != 0, out
    assert "docker could not run doxygen" in out, out
    assert "(exit 125)" in out, out
    assert "Cannot connect to the Docker daemon" in out, out
    assert "CI has moved" not in out, out


def test_a_different_version_in_the_image_still_says_ci_moved(
    tmp_path: Path,
) -> None:
    r = _run(tmp_path, docker='echo "1.10.0"\n')
    out = r.stdout + r.stderr
    assert r.returncode != 0, out
    assert f"ships doxygen 1.10.0, not {PIN}" in out, out
    assert "CI has moved" in out, out


def test_an_image_that_reports_no_version_is_named(tmp_path: Path) -> None:
    r = _run(tmp_path, docker="exit 0\n")
    out = r.stdout + r.stderr
    assert r.returncode != 0, out
    assert "reported no doxygen version" in out, out
    assert "CI has moved" not in out, out


def test_an_image_at_the_pin_runs_the_gate(tmp_path: Path) -> None:
    """The passing path: the image reports the pin, and the gate runs in it."""
    r = _run(
        tmp_path,
        docker=(
            'for a in "$@"; do\n'
            f'  [ "$a" = "--version" ] && {{ echo "{PIN}"; exit 0; }}\n'
            "done\n"
            "echo GATE-RAN\n"
        ),
    )
    out = r.stdout + r.stderr
    assert r.returncode == 0, out
    assert "GATE-RAN" in out, out
