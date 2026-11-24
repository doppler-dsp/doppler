"""Every tracked file of an example project reaches its docs page.

`scripts/gen_example_projects.py` writes `docs/example-projects/<name>.md`
from `git ls-files example-projects/<name>`: nothing is registered, so a file
added to a project cannot be forgotten, only left unregenerated, and
`--check` (docs-check and the Docs job) fails until it is regenerated
(doppler#2856: the pages were README-only, with no code and no way to get it).
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


def _gen():
    sys.path.insert(0, str(REPO / "scripts"))
    import gen_example_projects as g

    return g


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=root,
        check=True,
        capture_output=True,
    )


def _repo(tmp: Path) -> Path:
    """One project with a README, a build file and a source."""
    _git(tmp, "init", "-q")
    (tmp / "pyproject.toml").write_text('version = "1.2.3"\n')
    d = tmp / "example-projects" / "p"
    d.mkdir(parents=True)
    (d / "README.md").write_text(
        "# p -- a project\n\n<!-- --8<-- [start:body] -->\nBody.\n"
        "<!-- --8<-- [end:body] -->\n"
    )
    (d / "CMakeLists.txt").write_text("project(p C)\n")
    (d / "main.c").write_text("int main(void) { return 0; }\n")
    _git(tmp, "add", "-A")
    return tmp


def _write(g, root: Path) -> None:
    (root / "docs" / "example-projects").mkdir(parents=True, exist_ok=True)
    for rel, text in g.render(root).items():
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        (root / rel).write_text(text, encoding="utf-8")


def test_a_generated_page_includes_every_file_and_tells_how_to_get_it(
    tmp_path: Path,
) -> None:
    g = _gen()
    root = _repo(tmp_path)
    page = g.render(root)["docs/example-projects/p.md"]
    assert '--8<-- "example-projects/p/CMakeLists.txt"' in page
    assert '--8<-- "example-projects/p/main.c"' in page
    assert "README.md" not in page.split("## The files")[1]
    assert "archive/refs/tags/v1.2.3.tar.gz" in page
    assert "doppler-1.2.3/example-projects/p" in page
    assert "tree/v1.2.3/example-projects/p" in page
    assert page.index("CMakeLists.txt") < page.index(
        '--8<-- "example-projects/p/main.c"'
    )


def test_a_file_added_to_a_project_must_reach_its_page(tmp_path: Path) -> None:
    g = _gen()
    root = _repo(tmp_path)
    _write(g, root)
    assert g.missing_files(root) == []
    (root / "example-projects" / "p" / "extra.py").write_text("print(1)\n")
    _git(root, "add", "-A")
    miss = g.missing_files(root)
    assert miss and "example-projects/p/extra.py" in miss[0]
    # ...and regenerating reaches it
    _write(g, root)
    assert g.missing_files(root) == []


def test_a_readme_without_body_markers_is_refused_by_name(
    tmp_path: Path,
) -> None:
    g = _gen()
    root = _repo(tmp_path)
    (root / "example-projects" / "p" / "README.md").write_text("# p\n\nBody\n")
    with pytest.raises(SystemExit, match=r"example-projects/p/README\.md"):
        g.render(root)


def test_untracked_build_output_is_not_a_tab(tmp_path: Path) -> None:
    g = _gen()
    root = _repo(tmp_path)
    (root / "example-projects" / "p" / "build").mkdir()
    (root / "example-projects" / "p" / "build" / "a.out").write_text("x")
    assert all("build" not in f for f in g.project_files(root, "p"))


def test_every_tracked_file_of_every_real_project_is_on_its_page() -> None:
    g = _gen()
    assert g.projects(REPO), "no example projects found"
    assert g.missing_files(REPO) == []


def test_a_project_without_a_readme_is_refused_by_name(tmp_path: Path) -> None:
    g = _gen()
    root = _repo(tmp_path)
    (root / "example-projects" / "p" / "README.md").unlink()
    _git(root, "add", "-A")
    with pytest.raises(SystemExit, match=r"without a README.*\bp\b"):
        g.projects(root)


def test_an_orphan_page_is_found(
    tmp_path: Path,
) -> None:
    g = _gen()
    root = _repo(tmp_path)
    _write(g, root)
    assert g.page_problems(root) == []
    (root / "docs" / "example-projects" / "ghost.md").write_text("# ghost\n")
    assert any("ghost.md" in x for x in g.page_problems(root))


def test_a_binary_file_is_named_on_the_page_not_a_crash(
    tmp_path: Path,
) -> None:
    g = _gen()
    root = _repo(tmp_path)
    (root / "example-projects" / "p" / "data.bin").write_bytes(
        b"\xff\xfe\x00\x80"
    )
    _git(root, "add", "-A")
    _write(g, root)
    page = (root / "docs" / "example-projects" / "p.md").read_text(
        encoding="utf-8"
    )
    assert "data.bin" in page and "binary file" in page
    assert g.missing_files(root) == []


def _family(tmp: Path) -> Path:
    """The one-level project ``p`` beside a family ``fam`` of two."""
    root = _repo(tmp)
    for kid in ("a", "b"):
        d = root / "example-projects" / "fam" / kid
        d.mkdir(parents=True)
        (d / "README.md").write_text(
            f"# {kid}\n\n<!-- --8<-- [start:body] -->\nBody.\n"
            "<!-- --8<-- [end:body] -->\n"
        )
        (d / "Makefile").write_text("all:\n")
        (d / "main.cpp").write_text("int main() { return 0; }\n")
    _git(root, "add", "-A")
    return root


def test_a_family_is_a_project_per_child_with_its_own_page(
    tmp_path: Path,
) -> None:
    """A directory with no build file whose children have one is a family:
    each child is a project, paged under the family, fetched as itself.
    Rejects the one-level reading, which made ``fam`` one project whose
    children's files were tabs on a page nobody could build from."""
    g = _gen()
    root = _family(tmp_path)
    assert g.projects(root) == ["fam/a", "fam/b", "p"]
    pages = g.render(root)
    assert set(pages) == {
        "docs/example-projects/p.md",
        "docs/example-projects/fam/a.md",
        "docs/example-projects/fam/b.md",
    }
    a = pages["docs/example-projects/fam/a.md"]
    assert '--8<-- "example-projects/fam/a/main.cpp"' in a
    assert "```cpp" in a
    assert "example-projects/fam/b" not in a
    # the fetch strips down to the project's own folder, at either depth
    assert "--strip-components=3 doppler-1.2.3/example-projects/fam/a" in a
    p = pages["docs/example-projects/p.md"]
    assert "--strip-components=2 doppler-1.2.3/example-projects/p" in p
    _write(g, root)
    assert g.missing_files(root) == []
    assert g.page_problems(root) == []


def test_a_file_of_a_familys_own_is_refused_by_name(tmp_path: Path) -> None:
    """A family holds projects only: a file beside them would reach no page,
    so it is refused, naming it. Rejects it being silently dropped."""
    g = _gen()
    root = _family(tmp_path)
    (root / "example-projects" / "fam" / "notes.txt").write_text("x\n")
    _git(root, "add", "-A")
    with pytest.raises(SystemExit, match=r"fam/ is a family.*notes\.txt"):
        g.projects(root)


def test_an_orphan_page_under_a_family_is_found(tmp_path: Path) -> None:
    """A page left under a family's folder with no project behind it is an
    orphan too. Rejects an orphan check that reads one level only."""
    g = _gen()
    root = _family(tmp_path)
    _write(g, root)
    (root / "docs" / "example-projects" / "fam" / "gone.md").write_text("#\n")
    assert any("fam/gone.md" in x for x in g.page_problems(root))


def test_a_project_the_release_lacks_says_so_and_links_nothing(
    tmp_path: Path,
) -> None:
    """A project added since the release: its page has no fetch and no
    pinned link (both would 404 until the next release) and says it is new.
    Rejects the release block on a project the tag does not carry, which
    check_repo_links.py then refuses."""
    g = _gen()
    root = _repo(tmp_path)
    _git(root, "commit", "-q", "-m", "release")
    _git(root, "tag", "v1.2.3")
    d = root / "example-projects" / "q"
    d.mkdir()
    (d / "README.md").write_text(
        "# q\n\n<!-- --8<-- [start:body] -->\nBody.\n"
        "<!-- --8<-- [end:body] -->\n"
    )
    (d / "Makefile").write_text("all:\n")
    _git(root, "add", "-A")
    pages = g.render(root)
    q = pages["docs/example-projects/q.md"]
    assert "new since the latest release" in q
    assert "archive/refs/tags" not in q and "tree/v1.2.3" not in q
    assert '--8<-- "example-projects/q/Makefile"' in q
    p = pages["docs/example-projects/p.md"]
    assert "archive/refs/tags/v1.2.3.tar.gz" in p
