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


def _stamp():
    sys.path.insert(0, str(REPO / "scripts"))
    import gen_doc_versions as s

    return s


def test_the_stamper_leaves_an_archived_page_and_a_commit_link_alone() -> None:
    s = _stamp()
    old = PIN.replace("1.2.3", "0.1.0")
    text = f"[a]({old}/x)\n"
    assert "v9.9.9" in s.render(text, "9.9.9")
    assert s.render(text, "9.9.9", pin_links=False) == text
    # a commit-pinned link is never matched, in a live page either
    sha = f"https://github.com/doppler-dsp/doppler/blob/{'a' * 40}/x"
    assert s.render(sha, "9.9.9") == sha


def test_a_full_clone_is_not_made_shallow_by_the_tag_fetch(
    tmp_path: Path,
) -> None:
    """`git fetch --depth=1` on a FULL clone would cut its history."""
    sys.path.insert(0, str(REPO / "scripts"))
    import _release_tag as r

    origin = tmp_path / "origin"
    origin.mkdir()
    _repo(origin, "x\n", tag=True)
    clone = tmp_path / "clone"
    subprocess.run(
        ["git", "clone", "-q", "--no-tags", str(origin), str(clone)],
        check=True,
    )
    state, _ = r.tag_state(clone, "v1.2.3", ci=True)
    assert state == "present"
    shallow = r.git(clone, "rev-parse", "--is-shallow-repository").stdout
    assert shallow.strip() == "false"


def test_an_example_run_by_its_repository_path_is_refused(
    tmp_path: Path,
) -> None:
    """`python src/doppler/examples/x.py` works only in a clone; the wheel
    ships `python -m doppler.examples.x` (doppler#2857)."""
    for line in (
        "python src/doppler/examples/x.py",
        "uv run python src/doppler/examples/x.py --n 4",
        "python3 -u ./src/doppler/examples/x.py",
        ".venv/bin/python src/doppler/examples/x.py",
        "python3.12 src/doppler/examples/x.py",
    ):
        root = tmp_path / str(abs(hash(line)))
        root.mkdir()
        _repo(root, f"```sh\n{line}\n```\n")
        rc, out = _g().run(root, ci=True)
        assert rc == 1 and "repository path" in out, line
    ok = tmp_path / "ok"
    ok.mkdir()
    _repo(ok, "```sh\npython -m doppler.examples.x\n```\n")
    assert _g().run(ok, ci=True)[0] == 0
