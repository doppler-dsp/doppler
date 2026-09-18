"""The exported-link gate, exercised over seeded export files.

`scripts/check_exported_link_paths.py` exists because v0.51.0 exported
``find_library``'s ``/usr/lib64/libm.so`` into the installed
``doppler-targets.cmake``: every CMake consumer of the static library on
Debian or Ubuntu failed to link, while a consumer built on the build machine
linked fine because the path exists there. The defect is in the export file's
TEXT, so each case writes one and points the gate at it.

The pass cases matter as much as the fail cases: a library NAME and a path
under ``${_IMPORT_PREFIX}`` (the package's own file, relocated at the
consumer) are exactly what a correct export contains.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_exported_link_paths.py"


def _prefix(tmp_path: Path, link: str) -> Path:
    """An install prefix whose one export file carries *link*."""
    d = tmp_path / "lib" / "cmake" / "doppler"
    d.mkdir(parents=True)
    (d / "doppler-targets.cmake").write_text(
        "set_target_properties(doppler::doppler-static PROPERTIES\n"
        f'  INTERFACE_LINK_LIBRARIES "{link}"\n)\n',
        encoding="utf-8",
    )
    return tmp_path


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(root)],
        capture_output=True,
        text=True,
    )


def test_library_names_pass(tmp_path: Path) -> None:
    r = _run(_prefix(tmp_path, "m;Threads::Threads"))
    assert r.returncode == 0, r.stdout + r.stderr


def test_the_package_own_relocated_path_passes(tmp_path: Path) -> None:
    link = "${_IMPORT_PREFIX}/lib/libdoppler_stream.a;m"
    r = _run(_prefix(tmp_path, link))
    assert r.returncode == 0, r.stdout + r.stderr


def test_the_v0_51_0_export_fails(tmp_path: Path) -> None:
    """The exact value v0.51.0 shipped."""
    r = _run(_prefix(tmp_path, "Threads::Threads;/usr/lib64/libm.so"))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "/usr/lib64/libm.so" in r.stdout


def test_a_windows_drive_path_fails(tmp_path: Path) -> None:
    r = _run(_prefix(tmp_path, "C:/vcpkg/lib/zmq.lib"))
    assert r.returncode == 1, r.stdout + r.stderr


def test_a_tree_with_no_export_file_is_not_a_pass(tmp_path: Path) -> None:
    """A gate with nothing to read has not passed."""
    r = _run(tmp_path)
    assert r.returncode == 1, r.stdout + r.stderr
    assert "has not run" in r.stdout
