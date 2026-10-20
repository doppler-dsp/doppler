"""The src-untouched gate, exercised over a seeded tree.

`scripts/check_src_untouched.py` brackets `make warnings-check`'s two
throwaway builds: a snapshot of every compiled artifact under `src/` before,
a compare after. It exists because those builds wrote their clang-built
extensions and `wfmgen` into `src/doppler/` (doppler#2518), so every later
gate in the checkout measured that build instead of its own. Each case seeds
a tree with the shapes the builds leave there -- an extension module, the
`_bin/wfmgen` executable the package ships -- and runs the gate over it.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_src_untouched.py"

EXT = "doppler/track/track.cpython-313-x86_64-linux-gnu.so"
BIN = "doppler/wfm/_bin/wfmgen"
PY = "doppler/track/__init__.py"


def _elf(tag: str) -> bytes:
    """An ELF file's bytes, distinct per `tag`, as a compiler would differ."""
    return b"\x7fELF\x02\x01\x01" + tag.encode() * 16


@pytest.fixture
def src(tmp_path: Path) -> Path:
    root = tmp_path / "src"
    for rel, data in ((EXT, _elf("gcc")), (BIN, _elf("gcc-cli"))):
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    (root / PY).write_text("from .track import *\n", encoding="utf-8")
    return root


def _gate(mode: str, src: Path, snap: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(SCRIPT), mode, str(snap), "--root", str(src)],
        capture_output=True,
        text=True,
        check=False,
    )


def _snapshot(src: Path, tmp_path: Path) -> Path:
    snap = tmp_path / "snap.json"
    assert _gate("snapshot", src, snap).returncode == 0
    return snap


def test_untouched_tree_passes(src: Path, tmp_path: Path) -> None:
    snap = _snapshot(src, tmp_path)
    r = _gate("compare", src, snap)
    assert r.returncode == 0, r.stdout
    assert "2 compiled artifact(s)" in r.stdout


def test_an_extension_rebuilt_by_another_compiler_fails(
    src: Path, tmp_path: Path
) -> None:
    """The case #2518 was: the clang tree's module over the gcc one."""
    snap = _snapshot(src, tmp_path)
    (src / EXT).write_bytes(_elf("clang"))
    r = _gate("compare", src, snap)
    assert r.returncode == 1
    assert f"changed  {EXT}" in r.stdout


def test_the_shipped_executable_rewritten_fails(
    src: Path, tmp_path: Path
) -> None:
    """Not only *.so: wfmgen under _bin/ was overwritten too."""
    snap = _snapshot(src, tmp_path)
    (src / BIN).write_bytes(_elf("clang-cli"))
    r = _gate("compare", src, snap)
    assert r.returncode == 1
    assert f"changed  {BIN}" in r.stdout


def test_an_artifact_added_to_a_fresh_checkout_fails(tmp_path: Path) -> None:
    """CI's shape: a fresh checkout has none, so the gate is 'none added'."""
    root = tmp_path / "src"
    (root / "doppler").mkdir(parents=True)
    snap = _snapshot(root, tmp_path)
    ext = root / EXT
    ext.parent.mkdir(parents=True)
    ext.write_bytes(_elf("clang"))
    r = _gate("compare", root, snap)
    assert r.returncode == 1
    assert f"added    {EXT}" in r.stdout


def test_an_artifact_removed_fails(src: Path, tmp_path: Path) -> None:
    snap = _snapshot(src, tmp_path)
    (src / EXT).unlink()
    r = _gate("compare", src, snap)
    assert r.returncode == 1
    assert f"removed  {EXT}" in r.stdout


def test_an_artifact_is_known_by_its_magic_not_its_name(
    src: Path, tmp_path: Path
) -> None:
    """A compiled file with no telling suffix is still watched."""
    odd = src / "doppler" / "blob.bin"
    odd.write_bytes(_elf("gcc-odd"))
    snap = _snapshot(src, tmp_path)
    odd.write_bytes(_elf("clang-odd"))
    r = _gate("compare", src, snap)
    assert r.returncode == 1
    assert "changed  doppler/blob.bin" in r.stdout


def test_a_source_edit_during_the_build_is_not_the_builds(
    src: Path, tmp_path: Path
) -> None:
    """The builds take minutes; a .py edited meanwhile is the developer's."""
    snap = _snapshot(src, tmp_path)
    (src / PY).write_text("# edited\n", encoding="utf-8")
    r = _gate("compare", src, snap)
    assert r.returncode == 0, r.stdout
