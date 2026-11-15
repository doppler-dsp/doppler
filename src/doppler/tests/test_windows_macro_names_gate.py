"""The Windows-macro-name gate, exercised over a seeded tree.

`scripts/check_windows_macro_names.py` exists because `<windows.h>`
defines ``near``, ``IN`` and several other plain words as object-like
macros, and an identifier with one of those names broke only the Windows
CI legs, twice (PR #1975's ``IN``, PR #3071's ``near``).

Each case seeds a fake tree and points ``--root`` at it, because a gate
that can only run against the real tree cannot be sabotaged. The cases
past the two real defects guard against an eager gate: a comment or a
message may say "near", a longer identifier merely containing one of the
names is fine, and ``vendor/`` is not doppler's to fix.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_windows_macro_names.py"
TEST = "native/tests/a/test_a_core.c"


def _run(tmp_path: Path, body: str, rel: str = TEST):
    """Seed one C file at ``rel`` and run the gate over the tree."""
    p = tmp_path / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(body, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )


def test_a_clean_file_passes(tmp_path: Path) -> None:
    r = _run(tmp_path, "static const size_t b_at[3] = { 1, 2, 3 };\n")
    assert r.returncode == 0, r.stdout + r.stderr


def test_the_pool_test_near_fails(tmp_path: Path) -> None:
    """PR #3071's defect as found."""
    r = _run(tmp_path, "const size_t near[3] = { 999, 1002, 1005 };\n")
    assert r.returncode == 1
    assert "test_a_core.c:1: 'near' is a macro in minwindef.h" in r.stdout


def test_the_framer_enum_in_fails(tmp_path: Path) -> None:
    """PR #1975's defect as found: an enum constant local to one test."""
    r = _run(tmp_path, "enum { IN = 32, IH = 10 };\n")
    assert r.returncode == 1
    assert "'IN'" in r.stdout


def test_comments_and_literals_are_prose(tmp_path: Path) -> None:
    body = (
        "/* near the far end, IN and OUT */\n"
        'static const char *m = "too small";\n'
        "// interface\n"
        "static const char c = 'IN';\n"
    )
    r = _run(tmp_path, body)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_longer_identifier_is_not_the_name(tmp_path: Path) -> None:
    r = _run(tmp_path, "int nearest, farther, INPUT, smaller;\n")
    assert r.returncode == 0, r.stdout + r.stderr


def test_vendor_is_skipped(tmp_path: Path) -> None:
    r = _run(tmp_path, "int far;\n", rel="vendor/x/x.c")
    assert r.returncode == 0, r.stdout + r.stderr


EXEMPT = "scripts/.windows-macro-names-exempt"


def _exempt(tmp_path: Path, *pairs: str) -> None:
    p = tmp_path / EXEMPT
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(
        "# ratchet\n" + "".join(f"{x}\n" for x in pairs), encoding="utf-8"
    )


def test_a_listed_use_passes(tmp_path: Path) -> None:
    """A use that predates the gate is on the ratchet and passes."""
    _exempt(tmp_path, f"{TEST} near")
    r = _run(tmp_path, "static int near (float a, float b);\n")
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_listed_pair_covers_only_its_name(tmp_path: Path) -> None:
    """The file's listed ``near`` does not let a new ``far`` in."""
    _exempt(tmp_path, f"{TEST} near")
    r = _run(tmp_path, "static int near (int a);\nint far;\n")
    assert r.returncode == 1
    assert "'far'" in r.stdout


def test_a_stale_listed_pair_fails(tmp_path: Path) -> None:
    """Once the identifier is renamed its line must go: the list shrinks."""
    _exempt(tmp_path, f"{TEST} near")
    r = _run(tmp_path, "static int close_to (float a, float b);\n")
    assert r.returncode == 1
    assert "no longer used" in r.stdout


def test_a_wingdi_enum_constant_fails(tmp_path: Path) -> None:
    """wingdi.h's ERROR is 0: an enum constant named ERROR becomes 0 = …"""
    r = _run(tmp_path, "enum mode { OK, ERROR };\n")
    assert r.returncode == 1
    assert "'ERROR' is a macro in wingdi.h" in r.stdout


def test_an_include_path_is_not_an_identifier(tmp_path: Path) -> None:
    r = _run(tmp_path, '#include <far/x.h>\n#  include "near.h"\n')
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_malformed_exempt_line_is_named(tmp_path: Path) -> None:
    """A bad line says where it is, not a traceback."""
    _exempt(tmp_path, "native/a b c")
    r = _run(tmp_path, "int x;\n")
    assert r.returncode == 1
    assert f"{EXEMPT}:2: expected" in r.stderr
    assert "Traceback" not in r.stderr
