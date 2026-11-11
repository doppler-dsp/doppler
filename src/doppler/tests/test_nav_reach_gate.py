"""scripts/check_nav_reach.py: every docs/ page is in the built nav (#3000).

Each case builds a tiny ``docs/`` tree and the ``site/`` a build would make
from it -- one ``index.html`` per page whose primary nav links the pages the
case says the nav reaches -- so the gate is exercised on the artifact shape
it reads, without a zensical build.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "check_nav_reach.py"

YML = """site_name: t
exclude_docs: |
  /api.md
not_in_nav: |
  /c-api/*.md
  !/c-api/index.md
  **/archive/
"""


def _tree(root: Path, pages: list[str], navved: list[str], yml=YML) -> None:
    """``pages`` under docs/, each built; every built page's nav links the
    site paths of ``navved``."""
    (root / "mkdocs.yml").write_text(yml)
    site = root / "site"
    for rel in pages:
        md = root / "docs" / rel
        md.parent.mkdir(parents=True, exist_ok=True)
        md.write_text(f"# {rel}\n")
    links = "".join(
        f'<a class="md-nav__link" href="/{_url(r)}/">{r}</a>' for r in navved
    )
    html = (
        f'<nav class="md-nav md-nav--primary"><ul><li>{links}</li></ul></nav>'
    )
    for rel in pages:
        out = site / _url(rel) / "index.html"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(html)
    (site / "index.html").write_text(html)


def _url(rel: str) -> str:
    stem = rel[: -len(".md")]
    if stem == "index":
        return ""
    return stem[: -len("/index")] if stem.endswith("/index") else stem


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
        check=False,
    )


def test_every_page_in_the_nav_passes(tmp_path: Path) -> None:
    pages = ["index.md", "guide/index.md", "guide/a.md"]
    _tree(tmp_path, pages, pages)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stderr


def test_a_page_no_nav_reaches_fails_by_name(tmp_path: Path) -> None:
    """The case the glob exists for: a section that forgot it."""
    pages = ["index.md", "guide/index.md", "guide/a.md", "guide/new.md"]
    _tree(tmp_path, pages, pages[:3])
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "docs/guide/new.md: built, but no page's nav" in r.stderr
    assert "guide/a.md" not in r.stderr


def test_not_in_nav_excuses_exactly_what_it_names(tmp_path: Path) -> None:
    """c-api's per-file pages and archive/ are excused; c-api/index.md is
    re-included, so it still owes its nav entry."""
    pages = [
        "index.md",
        "c-api/index.md",
        "c-api/foo_8h.md",
        "dev/archive/old.md",
        "design/archive/older.md",
    ]
    _tree(tmp_path, pages, ["index.md", "c-api/index.md"])
    assert _run(tmp_path).returncode == 0
    _tree(tmp_path, pages, ["index.md"])
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "docs/c-api/index.md" in r.stderr
    assert "foo_8h" not in r.stderr and "archive" not in r.stderr


def test_an_excluded_doc_owes_nothing(tmp_path: Path) -> None:
    _tree(tmp_path, ["index.md", "api.md"], ["index.md"])
    assert _run(tmp_path).returncode == 0


def test_a_glob_does_not_cross_a_directory(tmp_path: Path) -> None:
    """`/c-api/*.md` excuses c-api's own pages, not a subdirectory's."""
    pages = ["index.md", "c-api/index.md", "c-api/sub/page.md"]
    _tree(tmp_path, pages, ["index.md", "c-api/index.md"])
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "docs/c-api/sub/page.md" in r.stderr


@pytest.mark.parametrize(
    "bad",
    [
        "not_in_nav:\n  - /c-api/*.md\n",  # a YAML list, not a | block
        "not_in_nav: |\n  c-api/*.md\n",  # unanchored: outside the subset
        "not_in_nav: |\n  /c-api/**/x.md\n",  # ** mid-pattern
    ],
)
def test_a_pattern_it_cannot_read_is_refused(tmp_path: Path, bad) -> None:
    _tree(tmp_path, ["index.md"], ["index.md"], yml="site_name: t\n" + bad)
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "mkdocs.yml" in r.stderr


def test_no_built_site_is_refused(tmp_path: Path) -> None:
    (tmp_path / "mkdocs.yml").write_text(YML)
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "run `make docs` first" in r.stderr
