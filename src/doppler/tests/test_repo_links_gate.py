"""The docs link the release, not main -- and the tag check cannot go quiet.

`scripts/check_repo_links.py` refuses `blob/main` links and unpinned
`git clone`s outside `docs/.main-links-allowed`, and holds every pinned link to
a file that exists in the tree AND at the release tag. The tag half is the one
that rots silently: skipped when the tag is missing it checks nothing in CI,
where checkouts carry no tags (doppler#2857). So the cases below cover each
state of the tag.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
PIN = "https://github.com/doppler-dsp/doppler/blob/v1.2.3"
MAIN = "https://github.com/doppler-dsp/doppler/blob/main"


def _g():
    sys.path.insert(0, str(REPO / "scripts"))
    import check_repo_links as g

    return g


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=root,
        check=True,
        capture_output=True,
    )


def _repo(tmp: Path, page: str = "", tag: bool = True) -> Path:
    """A repo at v1.2.3 holding src/a.c, with docs/p.md = ``page``."""
    _git(tmp, "init", "-q")
    (tmp / "pyproject.toml").write_text('version = "1.2.3"\n')
    (tmp / "src").mkdir()
    (tmp / "src" / "a.c").write_text("int a;\n")
    (tmp / "docs").mkdir()
    (tmp / "docs" / "p.md").write_text(page, encoding="utf-8")
    _git(tmp, "add", "-A")
    _git(tmp, "commit", "-q", "-m", "init")
    if tag:
        _git(tmp, "tag", "v1.2.3")
    return tmp


def test_a_pinned_link_to_a_real_file_passes(tmp_path: Path) -> None:
    root = _repo(tmp_path, f"[a]({PIN}/src/a.c)\n")
    rc, out = _g().run(root, ci=True)
    assert rc == 0, out
    assert "1 pinned link" in out and "v1.2.3 is local" in out


def test_a_main_link_is_refused_with_its_line(tmp_path: Path) -> None:
    root = _repo(tmp_path, f"x\n[a]({MAIN}/src/a.c)\n")
    rc, out = _g().run(root, ci=True)
    assert rc == 1 and "docs/p.md:2" in out and "`main`" in out


def test_a_page_the_allow_list_names_may_link_main(tmp_path: Path) -> None:
    root = _repo(tmp_path, f"[a]({MAIN}/src/a.c)\n")
    (root / "docs" / ".main-links-allowed").write_text(
        "docs/p.md about main itself\n"
    )
    rc, out = _g().run(root, ci=True)
    assert rc == 0, out


def test_an_allow_list_row_needs_a_reason(tmp_path: Path) -> None:
    root = _repo(tmp_path, "x\n")
    (root / "docs" / ".main-links-allowed").write_text("docs/p.md\n")
    try:
        _g().allowed(root)
    except SystemExit as e:
        assert "needs a reason" in str(e)
    else:
        raise AssertionError("a reasonless row was accepted")


def test_a_git_clone_needs_a_branch(tmp_path: Path) -> None:
    clone = "git clone https://github.com/doppler-dsp/doppler"
    root = _repo(tmp_path, f"```sh\n{clone}\n```\n")
    rc, out = _g().run(root, ci=True)
    assert rc == 1 and "without `--branch" in out
    (root / "docs" / "p.md").write_text(
        f"```sh\ngit clone --branch v1.2.3 {clone.split(' ', 2)[2]}\n```\n"
    )
    rc, out = _g().run(root, ci=True)
    assert rc == 0, out


def test_a_pinned_link_to_a_missing_file_fails(tmp_path: Path) -> None:
    root = _repo(tmp_path, f"[x]({PIN}/src/nope.c)\n")
    rc, out = _g().run(root, ci=True)
    assert rc == 1 and "src/nope.c" in out and "working tree" in out


def test_a_file_added_since_the_tag_would_404_and_fails(
    tmp_path: Path,
) -> None:
    root = _repo(tmp_path, "x\n")
    (root / "src" / "new.c").write_text("int n;\n")
    (root / "docs" / "p.md").write_text(f"[n]({PIN}/src/new.c)\n")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "after the tag")
    rc, out = _g().run(root, ci=True)
    assert rc == 1 and "not in v1.2.3" in out and "404" in out


def test_ci_without_the_tag_and_without_a_remote_fails_not_skips(
    tmp_path: Path,
) -> None:
    root = _repo(tmp_path, f"[a]({PIN}/src/a.c)\n", tag=False)
    rc, out = _g().run(root, ci=True)
    assert rc == 1 and "could not be fetched" in out and "failure" in out


def test_a_developer_offline_gets_a_loud_warning_and_a_pass(
    tmp_path: Path,
) -> None:
    root = _repo(tmp_path, f"[a]({PIN}/src/a.c)\n", tag=False)
    rc, out = _g().run(root, ci=False)
    assert rc == 0 and "WARNING" in out


def test_a_version_not_on_the_remote_is_unreleased_and_says_so(
    tmp_path: Path,
) -> None:
    work = tmp_path / "w"
    work.mkdir()
    root = _repo(work, f"[a]({PIN}/src/a.c)\n", tag=False)
    bare = tmp_path / "origin.git"
    subprocess.run(["git", "init", "-q", "--bare", str(bare)], check=True)
    _git(root, "remote", "add", "origin", str(bare))
    rc, out = _g().run(root, ci=True)
    assert rc == 0, out
    assert "not released yet" in out and "working tree only" in out


def test_the_real_tree_has_no_main_link_outside_the_allow_list() -> None:
    assert _g().unpinned(REPO) == []
