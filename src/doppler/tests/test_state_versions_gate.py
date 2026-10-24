"""The state-version gate, exercised over a seeded repository with tags.

`scripts/check_state_versions.py` refuses a state magic whose current version
names a layout some release already shipped. The case it was written for is
doppler#2569: the parallel pool took over the cell-receiver pool's magic
`'ADPL'`, which had shipped as v3, and restarted at v2 -- so its first bump
would have accepted every old blob of a matching size.

Each history is built as real commits and real tags, because the gate reads
`git tag` and the headers at each tag. The findings it must raise are
seeded, and so are the shapes it must NOT flag: an object unchanged since its
last release, a bump past every shipped version, and a magic no release has
carried. A gate that fires on those gets turned off.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "check_state_versions.py"
HEADER = "native/inc/doppler/obj/obj_core.h"


def _header(magic: str, version: str) -> str:
    a, b, c, d = magic
    return (
        f"#define OBJ_STATE_MAGIC DP_FOURCC ('{a}', '{b}', '{c}', '{d}')\n"
        f"#define OBJ_STATE_VERSION {version} /* a comment */\n"
    )


def _git(root: Path, *args: str) -> None:
    subprocess.run(["git", *args], cwd=root, check=True, capture_output=True)


def _repo(tmp_path: Path, history: list[tuple[str, str]]) -> Path:
    """A repo whose header carries `(tag, version)` at each tag in turn."""
    root = tmp_path / "repo"
    (root / HEADER).parent.mkdir(parents=True)
    _git(root, "init", "-q")
    _git(root, "config", "user.email", "t@example.com")
    _git(root, "config", "user.name", "t")
    for tag, version in history:
        # A full header text rather than a version: a tag's own spelling.
        text = version if "\n" in version else _header("ADPL", version)
        (root / HEADER).write_text(text)
        _git(root, "add", "-A")
        _git(root, "commit", "-q", "--allow-empty", "-m", tag)
        _git(root, "tag", tag)
    return root


def _run(
    root: Path, version: str | None = None, text: str | None = None
) -> subprocess.CompletedProcess[str]:
    if version is not None:
        (root / HEADER).write_text(_header("ADPL", version))
    if text is not None:
        (root / HEADER).write_text(text)
    return subprocess.run(
        [sys.executable, str(SCRIPT)], cwd=root, capture_output=True, text=True
    )


#: The real history of 'ADPL': v3 for the old pool, then v2 at v0.74.0.
POOL = [("v0.48.0", "3u"), ("v0.73.0", "3u"), ("v0.74.0", "2u")]


@pytest.mark.parametrize("version", ["2u", "3u"])
def test_a_version_a_release_shipped_is_refused(tmp_path, version):
    """2 is v0.74.0's own number, which a changed layout may not keep;
    3 is the old pool's -- the bump #2569 would have made."""
    r = _run(_repo(tmp_path, POOL), version)
    assert r.returncode == 1, r.stdout
    assert "'ADPL' is version" in r.stderr
    assert "set it to 4u" in r.stderr


def test_past_every_shipped_version_passes(tmp_path):
    r = _run(_repo(tmp_path, POOL), "4u")
    assert r.returncode == 0, r.stderr


def test_unchanged_since_the_release_passes(tmp_path):
    """The common case: nothing changed, so the version is the tag's."""
    root = _repo(tmp_path, [("v1.0.0", "1u"), ("v1.1.0", "2u")])
    assert _run(root, "2u").returncode == 0


def test_going_back_is_refused(tmp_path):
    root = _repo(tmp_path, [("v1.0.0", "1u"), ("v1.1.0", "2u")])
    r = _run(root, "1u")
    assert r.returncode == 1
    assert "v1.1.0 shipped 2" in r.stderr


def test_an_unreleased_magic_is_free(tmp_path):
    root = _repo(tmp_path, POOL)
    text = _header("ADPL", "4u") + _header("NEW_", "1u").replace(
        "OBJ_", "NEW_"
    )
    assert _run(root, text=text).returncode == 0


def test_a_magic_without_a_version_define_is_refused(tmp_path):
    """A literal version at the call site is invisible here, so it is a
    finding, not a skip."""
    root = _repo(tmp_path, POOL)
    r = _run(root, text=_header("ADPL", "4u").splitlines()[0] + "\n")
    assert r.returncode == 1
    assert "has no numeric OBJ_STATE_VERSION" in r.stderr


def test_a_magic_declared_twice_is_refused(tmp_path):
    root = _repo(tmp_path, POOL)
    other = root / "native/inc/doppler/other/other_core.h"
    other.parent.mkdir(parents=True)
    other.write_text(_header("ADPL", "4u"))
    r = _run(root, "4u")
    assert r.returncode == 1
    assert "already declared" in r.stderr


def test_no_tags_fails_closed(tmp_path):
    """With no history there is nothing to compare; that is not a pass."""
    root = _repo(tmp_path, POOL)
    for tag, _ in POOL:
        _git(root, "tag", "-d", tag)
    r = _run(root, "4u")
    assert r.returncode == 1
    assert "no release tags" in r.stderr


#: The framer through v0.74.0: no version define, the literal at the call.
LITERAL = (
    "#define OBJ_STATE_MAGIC DP_FOURCC ('F', 'R', 'M', 'R')\n"
    "    DP_GET_OPEN (OBJ_STATE_MAGIC, 1u, total);  \\\n"
    "    DP_SET_OPEN (OBJ_STATE_MAGIC, 1u, want);\n"
)


def _framer(version: str) -> str:
    return _header("FRMR", version)


@pytest.mark.parametrize(("version", "rc"), [("0u", 1), ("1u", 0), ("2u", 0)])
def test_a_call_site_literal_at_a_tag_is_its_version(tmp_path, version, rc):
    """Read as never released, FRMR would pass at 0u (the review of #2573).
    The tag's literal `1u` is its shipped version, so 0u is refused."""
    root = _repo(tmp_path, [("v1.0.0", LITERAL)])
    r = _run(root, text=_framer(version))
    assert r.returncode == rc, r.stderr
    if rc:
        assert "'FRMR' is version 0" in r.stderr


def test_a_tag_with_no_readable_version_fails_loud(tmp_path):
    """History the gate cannot read is not history it may assume empty."""
    blind = "#define OBJ_STATE_MAGIC DP_FOURCC ('F', 'R', 'M', 'R')\n"
    root = _repo(tmp_path, [("v1.0.0", blind)])
    r = _run(root, text=_framer("5u"))
    assert r.returncode == 1
    assert "'FRMR' shipped at v1.0.0..v1.0.0 with no version" in r.stderr
