"""`make ci-docs`: which diffs are docs-only, against the real declaration.

`scripts/ci_docs.py` answers ``code=false`` for a diff CI may treat as
docs-only, and every job docs cannot break then skips. So the dangerous
answer is a false ``code=false``. Each case commits one change to a real
temporary repository and runs the script with ``CI_DOCS_RE`` read from the
Makefile, so the test exercises the regex CI uses, not a copy of it.
"""

from __future__ import annotations

import re
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

ROOT = repo_root(__file__)
SCRIPT = ROOT / "scripts" / "ci_docs.py"


def _docs_re() -> str:
    """CI_DOCS_RE as make passes it: the Makefile's line, ``$$`` -> ``$``."""
    text = (ROOT / "Makefile").read_text(encoding="utf-8")
    m = re.search(r"^CI_DOCS_RE = (.+)$", text, re.M)
    assert m, "Makefile declares no CI_DOCS_RE"
    return m.group(1).replace("$$", "$")


def _git(repo: Path, *args: str) -> None:
    subprocess.run(["git", *args], cwd=repo, check=True, capture_output=True)


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    _git(tmp_path, "init", "-q", "-b", "main")
    _git(tmp_path, "config", "user.email", "t@t")
    _git(tmp_path, "config", "user.name", "t")
    for f in ("README.md", "docs/a.md", "native/src/x.c", "pyproject.toml"):
        (tmp_path / f).parent.mkdir(parents=True, exist_ok=True)
        (tmp_path / f).write_text("1\n")
    _git(tmp_path, "add", "-A")
    _git(tmp_path, "commit", "-q", "-m", "base")
    return tmp_path


def _classify(repo: Path, edit) -> dict[str, str]:
    edit(repo)
    _git(repo, "add", "-A")
    _git(repo, "commit", "-q", "-m", "change")
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--base", "HEAD^", "--re", _docs_re()],
        cwd=repo,
        capture_output=True,
        text=True,
        env={"PATH": "/usr/bin:/bin"},
    )
    assert r.returncode == 0, r.stderr
    return dict(line.split("=", 1) for line in r.stdout.split())


def _write(path: str, text: str = "2\n"):
    def edit(repo: Path) -> None:
        (repo / path).parent.mkdir(parents=True, exist_ok=True)
        (repo / path).write_text(text)

    return edit


def _rm(path: str):
    return lambda repo: (repo / path).unlink()


@pytest.mark.parametrize(
    "edit",
    [
        _write("docs/a.md"),
        _write("docs/new/page.md"),
        _write("README.md"),
        _write("mkdocs.yml"),
        _write("changelog.d/added/x.md"),
        _rm("docs/a.md"),
    ],
    ids=["edit", "add", "readme", "mkdocs", "fragment", "rm-page"],
)
def test_docs_only(repo: Path, edit) -> None:
    assert _classify(repo, edit) == {"docs": "true", "code": "false"}


@pytest.mark.parametrize(
    "edit",
    [
        _write("native/src/x.c"),
        _write("pyproject.toml"),
        _write("native/tests/README.md"),
        _rm("README.md"),
    ],
    ids=["c", "pyproject", "nested-readme", "rm-readme"],
)
def test_not_docs_only(repo: Path, edit) -> None:
    """Each would skip a job it can break if it read as docs-only.

    ``rm-readme`` is the deletion rule: pyproject reads README.md.
    """
    assert _classify(repo, edit)["code"] == "true"


def test_docs_plus_code_is_code(repo: Path) -> None:
    def both(r: Path) -> None:
        _write("docs/a.md")(r)
        _write("native/src/x.c")(r)

    assert _classify(repo, both) == {"docs": "true", "code": "true"}


def test_an_unreadable_base_runs_everything(repo: Path) -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--base", "nope", "--re", _docs_re()],
        cwd=repo,
        capture_output=True,
        text=True,
    )
    assert r.stdout.split() == ["docs=true", "code=true"]
