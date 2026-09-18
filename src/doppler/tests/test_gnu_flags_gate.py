"""The GCC-flag gate, exercised over a seeded tree.

`scripts/check_gnu_flags.py` exists because clang-cl drops a GCC-style
flag it does not share with only an `unknown argument ignored` warning:
native/validation/ asked for -O3, -funroll-loops and -ffp-contract=off in 35
target_compile_options() calls, and on Windows got none of them (#1360).

The pass cases are load-bearing: the helper call itself, the `/clang:`
spelling, `-O2` (both drivers accept it) and a generator expression must all
stay green, or the gate would push people off every correct form.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root
from doppler.tests.test_bare_libm_gate import _seed, _sub

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_gnu_flags.py"


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
    )


@pytest.mark.parametrize(
    "flag", ["-O3", "-Ofast", "-funroll-loops", "-ffp-contract=off", "-mfma"]
)
def test_a_gcc_only_flag_fails(tmp_path: Path, flag: str) -> None:
    body = f"target_compile_options(a PRIVATE {flag})\n"
    r = _run(_seed(tmp_path, _sub(body)))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "native/src/a/CMakeLists.txt:1" in r.stdout


def test_a_continuation_line_fails(tmp_path: Path) -> None:
    body = "target_compile_options(a PRIVATE\n    -O2 -ffast-math)\n"
    r = _run(_seed(tmp_path, _sub(body)))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "CMakeLists.txt:2" in r.stdout


@pytest.mark.parametrize(
    "body",
    [
        "dp_gnu_compile_options(a PRIVATE -O3 -funroll-loops)\n",
        "target_compile_options(a PRIVATE /clang:-O3)\n",
        "target_compile_options(a PRIVATE -O2)\n",
        "target_compile_options(a PRIVATE $<$<C_COMPILER_ID:GNU>:-O3>)\n",
        "target_compile_options(a PRIVATE b  # -O3 once, in prose\n    c)\n",
        "add_compile_options(-ffast-math)\n",
    ],
    ids=["helper", "clang-prefix", "O2", "genex", "comment", "global"],
)
def test_a_portable_spelling_passes(tmp_path: Path, body: str) -> None:
    r = _run(_seed(tmp_path, _sub(body)))
    assert r.returncode == 0, r.stdout + r.stderr
