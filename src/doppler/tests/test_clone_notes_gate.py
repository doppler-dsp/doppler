"""A page that runs a repository-only command carries a pinned clone.

`scripts/check_clone_notes.py` scans every page under docs/; a code fence that
runs `make <target>`, `./build/...` or `docker compose` needs a
`git clone --branch v<version>` on the same page, unless
`docs/.clone-note-exempt` lists it with a reason (doppler#2857, fix 5: the
wheel carries none of these, so a reader got `No rule to make target`).
"""

from __future__ import annotations

import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
CLONE = "git clone --branch v1.2.3 https://github.com/doppler-dsp/doppler"


def _g():
    sys.path.insert(0, str(REPO / "scripts"))
    import check_clone_notes as g

    return g


def _page(root: Path, rel: str, text: str) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


@pytest.mark.parametrize(
    "cmd",
    [
        "make build",
        "./build/native/examples/x",
        "docker compose up",
        "python scripts/plot_capture.py rx.tlm",
        "python3 scripts/check_x.py --list",
        "cmake -B build -DBUILD_PYTHON=ON",
        "cmake --build build",
        "bash tests/install/standalone.sh",
        "uv run pytest",
    ],
)
def test_a_repo_command_without_a_clone_fails(tmp_path: Path, cmd: str):
    _page(tmp_path, "docs/guide/x.md", f"run:\n\n```sh\n{cmd}\n```\n")
    found = _g().problems(tmp_path)
    assert len(found) == 1 and "docs/guide/x.md:4" in found[0]


def test_a_pinned_clone_satisfies_it(tmp_path: Path):
    _page(
        tmp_path,
        "docs/guide/x.md",
        f"```sh\n{CLONE}\n```\n\n```sh\nmake build\n```\n",
    )
    assert _g().problems(tmp_path) == []


def test_a_clone_after_the_command_does_not_satisfy_it(tmp_path: Path):
    """A reader follows a page top to bottom: the clone has to come first."""
    _page(
        tmp_path,
        "docs/guide/x.md",
        f"```sh\nmake build\n```\n\n```sh\n{CLONE}\n```\n",
    )
    found = _g().problems(tmp_path)
    assert len(found) == 1 and "is after it" in found[0]


def test_a_tilde_fence_is_a_fence(tmp_path: Path):
    _page(tmp_path, "docs/guide/x.md", "~~~sh\nmake build\n~~~\n")
    assert len(_g().problems(tmp_path)) == 1


def test_generated_c_api_pages_are_not_pages(tmp_path: Path):
    _page(tmp_path, "docs/c-api/x.md", "```sh\nmake build\n```\n")
    assert _g().problems(tmp_path) == []


def test_an_unpinned_clone_does_not(tmp_path: Path):
    _page(
        tmp_path,
        "docs/guide/x.md",
        "```sh\ngit clone https://github.com/doppler-dsp/doppler\n"
        "make build\n```\n",
    )
    assert len(_g().problems(tmp_path)) == 1


def test_prose_naming_a_command_is_not_a_command(tmp_path: Path):
    _page(tmp_path, "docs/guide/x.md", "run `make build` first\n")
    assert _g().problems(tmp_path) == []


def test_an_exempt_page_may_run_repo_commands(tmp_path: Path):
    _page(tmp_path, "docs/dev/y.md", "```sh\nmake test\n```\n")
    _page(tmp_path, "docs/.clone-note-exempt", "docs/dev/** contributors\n")
    assert _g().problems(tmp_path) == []


def test_an_exemption_needs_a_reason(tmp_path: Path):
    _page(tmp_path, "docs/.clone-note-exempt", "docs/dev/**\n")
    with pytest.raises(SystemExit):
        _g().problems(tmp_path)


def test_the_real_tree_passes():
    assert _g().problems(REPO) == []


def test_the_three_docs_gates_share_one_page_walk():
    """Page discovery and the exempt-file reader have one home (_docs_pages);
    a gate that grows its own copy drifts from the others."""
    for name in (
        "check_clone_notes",
        "check_broker_links",
        "check_repo_links",
    ):
        text = (REPO / "scripts" / f"{name}.py").read_text(encoding="utf-8")
        assert "from _docs_pages import" in text, name
        assert "rglob" not in text, f"{name} walks the pages itself"
