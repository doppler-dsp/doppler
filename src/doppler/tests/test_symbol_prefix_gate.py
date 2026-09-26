"""The symbol-prefix gate, exercised over seeded archives.

`scripts/check_symbol_prefix.py` reads `nm` over the two installed archives
and holds every export outside `dp_` to a ratchet that may only shrink
(#1545, burn-down #1565). The cases compile a one-function archive into a
scratch build directory and pair it with a scratch ratchet, because a gate
that can only be tested against the real tree cannot be sabotaged.
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
SCRIPT = REPO / "scripts" / "check_symbol_prefix.py"

pytestmark = pytest.mark.skipif(
    not (shutil.which("cc") and shutil.which("ar") and shutil.which("nm")),
    reason="needs cc, ar and nm to build a seeded archive",
)


def _archive(build: Path, source: str) -> None:
    """Compile ``source`` into ``build/libdoppler.a``."""
    build.mkdir(parents=True, exist_ok=True)
    (build / "x.c").write_text(source, encoding="utf-8")
    subprocess.run(
        ["cc", "-c", str(build / "x.c"), "-o", str(build / "x.o")],
        check=True,
    )
    subprocess.run(
        ["ar", "rcs", str(build / "libdoppler.a"), str(build / "x.o")],
        check=True,
    )


def _gate(build: Path, ratchet: Path) -> subprocess.CompletedProcess[str]:
    """Run the gate over ``build`` with ``ratchet`` as its list."""
    return subprocess.run(
        [sys.executable, str(SCRIPT), f"--ratchet={ratchet}"],
        capture_output=True,
        text=True,
        check=False,
        env={**os.environ, "DOPPLER_BUILD_DIR": str(build)},
    )


_SRC = (
    "int dp_ok_create (void) { return 0; }\n"
    "int legacy_name (void) { return 1; }\n"
    "static int hidden (void) { return 2; }\n"
    "int dp_uses_hidden (void) { return hidden (); }\n"
)


def test_passes_when_every_bare_export_is_listed(tmp_path: Path) -> None:
    """``dp_*`` and ``static`` need no entry; the listed bare name passes."""
    _archive(tmp_path / "build", _SRC)
    ratchet = tmp_path / "ratchet"
    ratchet.write_text("# header\nlegacy_name\n", encoding="utf-8")
    r = _gate(tmp_path / "build", ratchet)
    assert r.returncode == 0, r.stdout


def test_refuses_a_new_bare_export(tmp_path: Path) -> None:
    """The regression the gate exists for: an unprefixed public symbol."""
    _archive(
        tmp_path / "build", _SRC + "int fir_create (void) { return 3; }\n"
    )
    ratchet = tmp_path / "ratchet"
    ratchet.write_text("legacy_name\n", encoding="utf-8")
    r = _gate(tmp_path / "build", ratchet)
    assert r.returncode == 1
    assert "NEW bare export: fir_create" in r.stdout


def test_refuses_a_stale_entry(tmp_path: Path) -> None:
    """A renamed export must leave the list, or the ratchet cannot shrink."""
    _archive(tmp_path / "build", _SRC)
    ratchet = tmp_path / "ratchet"
    ratchet.write_text("legacy_name\nrenamed_since\n", encoding="utf-8")
    r = _gate(tmp_path / "build", ratchet)
    assert r.returncode == 1
    assert "STALE ratchet entry: renamed_since" in r.stdout


def test_an_unreadable_archive_is_not_a_pass(tmp_path: Path) -> None:
    """``nm`` failing reads no symbols; silence must not read as clean."""
    build = tmp_path / "build"
    build.mkdir()
    (build / "libdoppler.a").write_bytes(b"not an archive")
    ratchet = tmp_path / "ratchet"
    ratchet.write_text("", encoding="utf-8")
    r = _gate(build, ratchet)
    assert r.returncode != 0
