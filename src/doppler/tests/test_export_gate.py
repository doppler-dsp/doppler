"""The export gate, exercised over seeded shared libraries.

`scripts/check_exports.py` holds each shared library to the public headers in
both directions (doppler#1164): nothing it exports may be absent from them,
and nothing they publish that its archive defines may be missing from its
exports. The cases compile a small library pair into a scratch build
directory and hand the gate a seeded public list, because a gate that can
only be tested against the real tree cannot be sabotaged.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_exports.py"

pytestmark = pytest.mark.skipif(
    not (
        sys.platform.startswith("linux")
        and shutil.which("cc")
        and shutil.which("ar")
        and shutil.which("nm")
    ),
    reason="needs an ELF toolchain: cc, ar and nm",
)

_SRC = (
    "int dp_public_create (void) { return 0; }\n"
    "int dp_public_step (void) { return 1; }\n"
    "int cJSON_Parse (void) { return 2; }\n"
)
_PUBLIC = "# seeded\ndp_public_create function\ndp_public_step function\n"


def _libs(build: Path, exports: list[str] | None) -> None:
    """Build ``libdoppler.a`` and ``libdoppler.so`` from ``_SRC``.

    ``exports`` is the version script's global list; ``None`` links with no
    script, which exports everything -- the state before #1164.
    """
    build.mkdir(parents=True, exist_ok=True)
    (build / "x.c").write_text(_SRC, encoding="utf-8")
    obj = build / "x.o"
    subprocess.run(
        ["cc", "-fPIC", "-c", str(build / "x.c"), "-o", str(obj)], check=True
    )
    subprocess.run(
        ["ar", "rcs", str(build / "libdoppler.a"), str(obj)], check=True
    )
    link = ["cc", "-shared", str(obj), "-o", str(build / "libdoppler.so")]
    if exports is not None:
        names = "".join(f"{n}; " for n in exports)
        (build / "x.map").write_text(
            f"{{ global: {names}local: *; }};\n", encoding="utf-8"
        )
        link.append(f"-Wl,--version-script={build / 'x.map'}")
    subprocess.run(link, check=True)


def _gate(build: Path) -> subprocess.CompletedProcess[str]:
    (build / "public.txt").write_text(_PUBLIC, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--public", str(build / "public.txt")],
        capture_output=True,
        text=True,
        check=False,
        env={**os.environ, "DOPPLER_BUILD_DIR": str(build)},
    )


def test_passes_when_exports_are_the_public_list(tmp_path: Path) -> None:
    """The shape the build writes: the public names, everything else local."""
    _libs(tmp_path, ["dp_public_create", "dp_public_step"])
    r = _gate(tmp_path)
    assert r.returncode == 0, r.stdout


def test_refuses_a_leaked_vendored_symbol(tmp_path: Path) -> None:
    """No export list -- every external symbol leaks, cJSON among them."""
    _libs(tmp_path, None)
    r = _gate(tmp_path)
    assert r.returncode == 1
    assert "exports cJSON_Parse (vendored)" in r.stdout


def test_refuses_a_missing_public_symbol(tmp_path: Path) -> None:
    """A published, defined function the library does not export."""
    _libs(tmp_path, ["dp_public_create"])
    r = _gate(tmp_path)
    assert r.returncode == 1
    assert "does not export dp_public_step" in r.stdout


def test_no_library_is_not_a_pass(tmp_path: Path) -> None:
    """An empty build directory fails rather than reporting nothing."""
    r = _gate(tmp_path)
    assert r.returncode == 1
    assert "has not passed" in r.stdout
