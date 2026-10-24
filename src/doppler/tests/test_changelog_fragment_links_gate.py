"""Changelog links are judged from the repository root (#2571).

`changelog-assemble` moves each `changelog.d/` fragment verbatim into the
root `CHANGELOG.md`. A link spelled from the fragment's own directory,
`../../docs/...`, is right in the fragment's PR and points outside the
repository once assembled. That shipped in v0.74.0's release commit and went
red on every CI leg there, after its own PR had passed. So
`scripts/check_doc_paths.py` reads a fragment's links from the root, and
`CHANGELOG.md`'s pre-existing broken links sit on a list that only shrinks.

Each case is seeded in a throwaway tree and run through the gate's own
function.
"""

from __future__ import annotations

import importlib.util
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

_SPEC = importlib.util.spec_from_file_location(
    "check_doc_paths", repo_root(__file__) / "scripts" / "check_doc_paths.py"
)
assert _SPEC is not None and _SPEC.loader is not None
gate = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(gate)


def _tree(
    tmp_path: Path,
    changelog: str,
    exempt: str = "",
    fragment: str | None = None,
) -> Path:
    (tmp_path / "docs" / "design").mkdir(parents=True)
    (tmp_path / "docs" / "design" / "page.md").write_text("# Page\n")
    (tmp_path / "scripts").mkdir()
    (tmp_path / "scripts" / ".changelog-link-exempt").write_text(
        "# header\n" + exempt
    )
    (tmp_path / "CHANGELOG.md").write_text(changelog)
    if fragment is not None:
        frag = tmp_path / "changelog.d" / "fixed" / "x.md"
        frag.parent.mkdir(parents=True)
        frag.write_text(fragment)
    return tmp_path


def _findings(root: Path) -> list[str]:
    failures: list[str] = []
    gate._changelog_links(failures, root)
    return failures


@pytest.mark.parametrize(
    ("link", "ok"),
    [
        ("docs/design/page.md", True),
        ("../../docs/design/page.md", False),
        ("docs/design/gone.md", False),
    ],
    ids=["from-the-root", "from-the-fragment", "missing"],
)
def test_a_fragment_link_resolves_from_the_root(tmp_path, link, ok):
    root = _tree(
        tmp_path,
        "# Changelog\n",
        fragment=f"- **X.** See [the page]({link}).\n",
    )
    found = _findings(root)
    assert (not found) is ok, found
    assert all(f.startswith("changelog.d/fixed/x.md: ") for f in found)


def test_an_exempt_changelog_link_passes(tmp_path):
    root = _tree(
        tmp_path,
        "See [x](../../docs/design/page.md).\n",
        exempt="../../docs/design/page.md\n",
    )
    assert _findings(root) == []


def test_a_new_broken_changelog_link_is_caught(tmp_path):
    """A second copy of an exempt target is still a finding."""
    root = _tree(
        tmp_path,
        "See [x](../../docs/design/page.md) and "
        "[y](../../docs/design/page.md).\n",
        exempt="../../docs/design/page.md\n",
    )
    (f,) = _findings(root)
    assert "(1x)" in f


def test_a_stale_exemption_is_caught(tmp_path):
    """The list only shrinks: a fixed link must leave it."""
    root = _tree(
        tmp_path,
        "See [x](docs/design/page.md).\n",
        exempt="../../docs/design/page.md\n",
    )
    (f,) = _findings(root)
    assert "no longer occurs" in f


def test_the_readme_is_not_a_fragment(tmp_path):
    root = _tree(tmp_path, "# Changelog\n")
    readme = root / "changelog.d" / "README.md"
    readme.parent.mkdir(parents=True)
    readme.write_text("See [the guide](../docs/design/page.md).\n")
    assert _findings(root) == []
