"""The installed-headers gate, exercised over seeded install manifests.

`cmake/check_installed_headers.cmake` is what `make package-c` runs after
`cmake --install`: every header THIS install wrote must sit under
`<prefix>/include/doppler/` (doppler#1408). It used to list
`<prefix>/include` instead, which refused every shared prefix -- `~/.local`,
the Makefile's own default `/usr/local` -- the moment another package, or an
older flat doppler install, had put a header there (doppler#1543).

The cases seed a manifest rather than install, because the real install
only regresses when CMakeLists does: a gate that can only be tested against
the real tree cannot be sabotaged.
"""

from __future__ import annotations

import shutil
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "cmake" / "check_installed_headers.cmake"


def _check(
    tmp_path: Path, prefix: Path, installed: list[str]
) -> subprocess.CompletedProcess[str]:
    """Run the gate over a manifest listing ``installed`` under ``prefix``.

    CMake writes its manifest with no trailing newline, so this does too.
    """
    cmake = shutil.which("cmake")
    assert cmake, "cmake is not on PATH; the gate cannot run without it"
    manifest = tmp_path / "install_manifest.txt"
    manifest.write_text(
        "\n".join(str(prefix / p) for p in installed), encoding="utf-8"
    )
    return subprocess.run(
        [
            cmake,
            f"-DMANIFEST={manifest}",
            f"-DPREFIX={prefix}",
            "-P",
            str(SCRIPT),
        ],
        capture_output=True,
        text=True,
        check=False,
    )


_GOOD = [
    "include/doppler/doppler.h",
    "include/doppler/fft/fft_core.h",
    "lib/libdoppler.a",
    "lib/pkgconfig/doppler.pc",
    "lib/cmake/doppler/doppler-config.cmake",
]


def test_passes_a_shared_prefix(tmp_path: Path) -> None:
    """Other packages' headers in the prefix are not doppler's to judge."""
    prefix = tmp_path / "local"
    (prefix / "include" / "otherlib").mkdir(parents=True)
    (prefix / "include" / "zlib.h").touch()
    (prefix / "include" / "fft").mkdir()  # a stale pre-#1408 install
    r = _check(tmp_path, prefix, _GOOD)
    assert r.returncode == 0, r.stderr


def test_refuses_a_flat_header(tmp_path: Path) -> None:
    """The #1408 regression: a header installed at the prefix root."""
    prefix = tmp_path / "pfx"
    r = _check(
        tmp_path,
        prefix,
        [*_GOOD, "include/fft/fft_core.h", "include/doppler.h"],
    )
    assert r.returncode != 0
    assert "2 header(s) installed outside include/doppler/" in r.stderr
    assert "include/fft/fft_core.h" in r.stderr
    assert "include/doppler.h" in r.stderr


def test_refuses_a_missing_manifest(tmp_path: Path) -> None:
    """No manifest is no evidence, never a pass."""
    cmake = shutil.which("cmake")
    assert cmake
    r = subprocess.run(
        [
            cmake,
            f"-DMANIFEST={tmp_path / 'absent.txt'}",
            f"-DPREFIX={tmp_path}",
            "-P",
            str(SCRIPT),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode != 0
    assert "no install manifest" in r.stderr
