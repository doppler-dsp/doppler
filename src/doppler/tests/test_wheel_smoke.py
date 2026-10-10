"""tests/install/wheel-smoke.sh --pypi, against a stub uv: no network.

The post-publish smoke is the only thing in a release that installs from the
index, so what it refuses is the point of it. Three properties, each of which
has failed for real:

- **The install retries itself while the index catches up** (doppler#1394).
  v0.51.1's smoke waited on a ``--dry-run`` resolve that succeeded, then the
  real install 0.2 s later got an index without the release. The install is
  now the readiness check, so the request that sees the release installs it.
- **A wheel, never the sdist** (doppler#1817). Without ``--only-binary`` the
  resolver falls back to the sdist when no wheel fits, and builds it, so the
  smoke passes on the very defect -- uninstallable ``cp313-cpwin_amd64``
  wheels -- that it exists to catch.
- **The venv is the Python asked for**, so a leg cannot quietly test another
  Python's wheel.

The stub ``uv`` makes a venv whose ``python`` answers the script's three
probes, and fails the first ``$FAIL_N`` real installs with ``$ERR``. Every
``uv pip install`` is appended to ``$CALLS``.
"""

from __future__ import annotations

import os
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._platform import skip_without_posix_shell
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "tests" / "install" / "wheel-smoke.sh"
VERSION = "0.65.0"
NOT_YET = (
    "Because there is no version of doppler-dsp==0.65.0 and you require "
    "doppler-dsp==0.65.0, we can conclude that your requirements are "
    "unsatisfiable."
)

#: The venv's interpreter: the Python version, the installed doppler version,
#: the WHEEL tag line, and the end-to-end run (any script path: exit 0).
_PYTHON = """#!/usr/bin/env bash
if [ "$1" = -c ]; then
  case "$2" in
    *sys.version_info*) echo "$PY_VERSION" ;;
    *doppler.__version__*) echo "$DOPPLER_VERSION" ;;
    *importlib.metadata*) echo "   wheel cp313-cp313-manylinux_2_28_x86_64" ;;
  esac
  exit 0
fi
exit 0
"""

#: `uv venv [--quiet] [--python X] DIR` makes DIR/bin/python from $PY_STUB;
#: `uv pip install …` is logged, and the first $FAIL_N real installs fail.
_UV = """#!/usr/bin/env bash
if [ "$1" = venv ]; then
  dir="${@: -1}"
  mkdir -p "$dir/bin"
  cp "$PY_STUB" "$dir/bin/python"
  chmod +x "$dir/bin/python"
  exit 0
fi
if [ "$1" = pip ] && [ "$2" = install ]; then
  echo "$*" >> "$CALLS"
  case " $* " in *" --dry-run "*) exit 0 ;; esac
  n=$(grep -cv -- --dry-run "$CALLS")
  if [ "$n" -le "$FAIL_N" ]; then echo "$ERR" >&2; exit 1; fi
  exit 0
fi
echo "stub uv: unexpected $*" >&2
exit 2
"""


def _smoke(
    tmp_path: Path,
    *,
    fail_n: int = 0,
    attempts: int = 5,
    py_version: str = "3.13",
    doppler_version: str = VERSION,
    err: str = NOT_YET,
):
    """Run the --pypi face for 3.13 against the stub; return it and the
    logged `uv pip install` calls."""
    skip_without_posix_shell("tests/install/wheel-smoke.sh")
    stubs = tmp_path / "stubs"
    stubs.mkdir()
    (stubs / "uv").write_text(_UV, encoding="utf-8")
    (stubs / "uv").chmod(0o755)
    py = tmp_path / "python-stub"
    py.write_text(_PYTHON, encoding="utf-8")
    calls = tmp_path / "calls"
    calls.write_text("", encoding="utf-8")
    env = {
        **os.environ,
        "PATH": f"{stubs}{os.pathsep}{os.environ['PATH']}",
        "PY_STUB": str(py),
        "CALLS": str(calls),
        "FAIL_N": str(fail_n),
        "ERR": err,
        "PY_VERSION": py_version,
        "DOPPLER_VERSION": doppler_version,
        "WHEEL_SMOKE_ATTEMPTS": str(attempts),
        "WHEEL_SMOKE_DELAY": "0",
    }
    r = subprocess.run(
        ["bash", str(SCRIPT), "--pypi", VERSION, "--python", "3.13"],
        capture_output=True,
        text=True,
        env=env,
    )
    return r, calls.read_text(encoding="utf-8").splitlines()


def test_the_install_is_retried_until_the_index_serves_the_release(
    tmp_path: Path,
) -> None:
    """doppler#1394: the request that waits is the request that installs."""
    r, calls = _smoke(tmp_path, fail_n=2)
    assert r.returncode == 0, r.stderr
    assert len(calls) == 3, calls
    assert not [c for c in calls if "--dry-run" in c], (
        "a dry-run readiness gate is a second request, which can see a "
        "different CDN edge than the install (doppler#1394)"
    )


def test_every_install_refuses_the_sdist(tmp_path: Path) -> None:
    """doppler#1817: without --only-binary the sdist builds and passes."""
    r, calls = _smoke(tmp_path, fail_n=1)
    assert r.returncode == 0, r.stderr
    assert calls
    for c in calls:
        assert "--only-binary doppler-dsp" in c, c


def test_an_install_that_never_succeeds_fails_bounded_in_uvs_words(
    tmp_path: Path,
) -> None:
    """Bounded, and the reader gets uv's own error rather than a guess at
    which of the two causes (index lag, no fitting wheel) it was."""
    why = "doppler-dsp==0.65.0 has no wheels with a matching platform tag"
    r, calls = _smoke(tmp_path, fail_n=99, attempts=3, err=why)
    assert r.returncode == 1
    assert len(calls) == 3, calls
    assert "could not install doppler-dsp==0.65.0" in r.stderr
    assert why in r.stderr


def test_a_venv_on_another_python_fails_before_installing(
    tmp_path: Path,
) -> None:
    """A leg asked for 3.13 that got 3.12 would test 3.12's wheel."""
    r, calls = _smoke(tmp_path, py_version="3.12")
    assert r.returncode == 1
    assert "venv is Python 3.12, asked for 3.13" in r.stderr
    assert calls == []


def test_the_installed_version_must_be_the_release(tmp_path: Path) -> None:
    """The resolver picks the version, so the smoke checks it picked this."""
    r, _ = _smoke(tmp_path, doppler_version="0.64.0")
    assert r.returncode == 1
    assert f"installed 0.64.0, expected {VERSION}" in r.stderr
