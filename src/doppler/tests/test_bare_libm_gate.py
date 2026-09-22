"""The bare-libm gate, exercised over a seeded tree.

`scripts/check_bare_libm.py` exists because doppler's C library first
compiled on Windows and then could not link: libm lives inside the UCRT
there, so a bare `m` in `target_link_libraries` is a request for `m.lib`,
which does not exist. 85 hand-written link lines and eight manifest
`extra_link_libs = ["m"]` entries still named it that way (doppler#1364), and
every CMake tutorial writes it like that -- it works on the platform doing
the writing.

Each case seeds a fake tree and points `--root` at it, because a gate that
can only run against the real tree cannot be sabotaged.

Three cases guard the gate against being too eager, and all three are
load-bearing. `mvec` and a `$<$<PLATFORM_ID:Linux>:m>` generator expression
are not bare -- the second is precisely the Windows-safe way to say "libm on
Linux only". Prose about libm in a comment is not a link. And a SEPARATE
CMake project in the tree must be left alone: `${DP_MATH_LIBRARY}` is
resolved in doppler's root, so rewriting another project's `m` would
silently drop libm on Linux and break a link that works today.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_bare_libm.py"

ROOT_CML = "cmake_minimum_required(VERSION 3.16)\nproject(doppler C)\n"


def _seed(tmp_path: Path, files: dict[str, str]) -> Path:
    """A root project plus whatever CMake files the case needs."""
    (tmp_path / "CMakeLists.txt").write_text(
        ROOT_CML + files.pop("CMakeLists.txt", ""), encoding="utf-8"
    )
    for rel, body in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")
    return tmp_path


def _run(root: Path, *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), *extra],
        capture_output=True,
        text=True,
    )


def _sub(body: str) -> dict[str, str]:
    return {"native/src/a/CMakeLists.txt": body}


def test_the_resolved_variable_passes(tmp_path: Path) -> None:
    body = "target_link_libraries(a PRIVATE a_core ${DP_MATH_LIBRARY})\n"
    r = _run(_seed(tmp_path, _sub(body)))
    assert r.returncode == 0, r.stdout + r.stderr


def test_m_as_the_last_item_fails(tmp_path: Path) -> None:
    """The common form, and the one a narrow first survey missed."""
    r = _run(_seed(tmp_path, _sub("target_link_libraries(a PRIVATE b m)\n")))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "native/src/a/CMakeLists.txt:1" in r.stdout


def test_m_as_the_only_item_fails(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, _sub("target_link_libraries(a PUBLIC m)\n")))
    assert r.returncode == 1, r.stdout + r.stderr


def test_m_on_its_own_line_in_a_multiline_call_fails(tmp_path: Path) -> None:
    body = "target_link_libraries(a\n    PRIVATE b\n    c\n    m)\n"
    r = _run(_seed(tmp_path, _sub(body)))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "CMakeLists.txt:4" in r.stdout


def test_a_longer_name_is_not_bare_m(tmp_path: Path) -> None:
    body = "target_link_libraries(a PRIVATE mvec mm m_core)\n"
    r = _run(_seed(tmp_path, _sub(body)))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_platform_generator_expression_is_not_bare_m(
    tmp_path: Path,
) -> None:
    """`$<$<PLATFORM_ID:Linux>:m>` is the Windows-SAFE spelling."""
    body = "target_link_libraries(a PRIVATE $<$<PLATFORM_ID:Linux>:m>)\n"
    r = _run(_seed(tmp_path, _sub(body)))
    assert r.returncode == 0, r.stdout + r.stderr


def test_prose_in_a_comment_is_not_a_link(tmp_path: Path) -> None:
    """The comment sits INSIDE an open call -- a trailing one after `)` is
    never scanned, so it could not tell a broken comment strip from a
    working one."""
    body = (
        "target_link_libraries(a\n"
        "    PRIVATE b   # m arrives through the variable below\n"
        "    ${DP_MATH_LIBRARY})\n"
    )
    r = _run(_seed(tmp_path, _sub(body)))
    assert r.returncode == 0, r.stdout + r.stderr


def test_m_outside_a_link_call_is_not_a_link(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, _sub("set(SUFFIXES m s)\n")))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_separate_project_is_left_alone(tmp_path: Path) -> None:
    """DP_MATH_LIBRARY does not exist there; rewriting would drop libm."""
    files = {
        "example-projects/demo/CMakeLists.txt": (
            "project(demo C)\ntarget_link_libraries(demo PRIVATE m)\n"
        ),
    }
    root = _seed(tmp_path, files)
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr
    r = _run(root, "--fix")
    assert "PRIVATE m)" in (
        root / "example-projects/demo/CMakeLists.txt"
    ).read_text(encoding="utf-8")


def test_fix_rewrites_every_form_and_then_passes(tmp_path: Path) -> None:
    body = (
        "target_link_libraries(a PUBLIC m)\n"
        "target_link_libraries(b PRIVATE b_core m)\n"
        "target_link_libraries(c\n    PRIVATE c_core\n    m)\n"
    )
    root = _seed(tmp_path, _sub(body))
    assert _run(root, "--fix").returncode == 0
    text = (root / "native/src/a/CMakeLists.txt").read_text(encoding="utf-8")
    assert text.count("${DP_MATH_LIBRARY}") == 3, text
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr
