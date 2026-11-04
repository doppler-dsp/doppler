"""The domain tree is gated: each of its rules goes red when broken.

``docs/domains.toml`` names the one grouping the docs use (a domain is a
Python module with a task title). ``scripts/gen_domains.py`` holds its rules
and ``scripts/check_user_docs.py`` runs them on ``make lint``. These cases run
the real script on a synthetic tree and break each rule in turn.
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
SCRIPT = REPO / "scripts" / "check_user_docs.py"
EX = "src/doppler/examples"
TOML = "docs/domains.toml"
GUIDES = "docs/.domain-guides-exempt"
FM = "<!-- domain: sig -->\n\n"
TABLE = (
    '[domain.sig]\ntitle = "Signal widgets (sig)"\narea = "sources"\n'
    'summary = "Widgets."\n'
)


def _w(root: Path, rel: str, text: str) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def _fixture(root: Path) -> Path:
    """One module, one class, fully documented and filed under `sig`."""
    _w(root, "src/doppler/sig/__init__.py", '__all__ = ["Widget"]\n')
    _w(root, "src/doppler/sig/sig.pyi", "class Widget: ...\n")
    _w(root, "just-makeit.toml", 'record_name = "WidgetStats"\n')
    _w(root, "docs/api/python-sig.md", "# sig\n\n::: doppler.sig.Widget\n")
    _w(root, "docs/guide/widgets.md", FM + "# Widgets\n\nUse `Widget`.\n")
    _w(root, "docs/gallery/widget.md", FM + "# A widget\n")
    _w(root, "docs/design/widget.md", FM + "# Why a widget\n")
    _w(root, f"{EX}/widget_demo.py", "from doppler.sig import Widget\n")
    _w(root, f"{EX}/.no-figure", "widget_demo.py: prints a number\n")
    _w(root, TOML, TABLE)
    return root


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), "--no-ratchet"],
        capture_output=True,
        text=True,
        check=False,
    )


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=root,
        check=True,
        capture_output=True,
    )


def test_a_sound_tree_passes(tmp_path: Path) -> None:
    r = _run(_fixture(tmp_path))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_module_missing_from_the_table_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, TOML, "")
    r = _run(root)
    assert r.returncode == 1
    assert "module `sig` has no [domain.sig] table" in r.stderr


def test_a_table_for_no_module_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, TOML, TABLE + '\n[domain.ghost]\ntitle = "Ghost"\n')
    r = _run(root)
    assert r.returncode == 1
    assert "[domain.ghost] names no public module" in r.stderr


def test_a_typo_cannot_create_an_area(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, TOML, TABLE.replace('"sources"', '"sorces"'))
    r = _run(root)
    assert r.returncode == 1
    assert "area 'sorces' is not one of" in r.stderr


@pytest.mark.parametrize("key", ["title", "area", "summary"])
def test_a_table_needs_title_area_and_summary(
    tmp_path: Path, key: str
) -> None:
    root = _fixture(tmp_path)
    keep = [ln for ln in TABLE.splitlines() if not ln.startswith(key)]
    _w(root, TOML, "\n".join(keep) + "\n")
    r = _run(root)
    assert r.returncode == 1
    assert f"lacks ['{key}']" in r.stderr


def test_a_title_that_drifts_from_its_module_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, TOML, TABLE.replace("Signal widgets (sig)", "Gadgets"))
    r = _run(root)
    assert r.returncode == 1
    assert "names neither the module nor a synonym" in r.stderr
    # a declared synonym is the escape hatch, and only that
    _w(
        root,
        TOML,
        TABLE.replace("Signal widgets (sig)", "Gadgets")
        + 'synonyms = ["gadgets"]\n',
    )
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr


@pytest.mark.parametrize("fam", ["guide", "gallery", "design"])
def test_a_page_with_no_domain_fails(tmp_path: Path, fam: str) -> None:
    root = _fixture(tmp_path)
    page = {"guide": "widgets", "gallery": "widget", "design": "widget"}[fam]
    _w(root, f"docs/{fam}/{page}.md", "# No domain line\n\nUse `Widget`.\n")
    r = _run(root)
    assert r.returncode == 1
    assert f"docs/{fam}/{page}.md: no domain" in r.stderr


def test_a_page_with_an_unknown_domain_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "docs/gallery/widget.md", "<!-- domain: nope -->\n\n# W\n")
    r = _run(root)
    assert r.returncode == 1
    assert "domain `nope` is not a module in docs/domains.toml" in r.stderr


def test_an_index_page_and_an_archive_owe_no_domain(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "docs/guide/index.md", "# Guides\n")
    _w(root, "docs/design/archive/old.md", "# Old\n")
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_module_with_classes_needs_a_guide(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    # the guide moves to another domain; `sig` has classes and no guide
    _w(
        root,
        TOML,
        TABLE + '\n[domain.other]\ntitle = "Other"\n'
        'area = "sources"\nsummary = "x."\n',
    )
    _w(root, "src/doppler/other/__init__.py", "__all__ = []\n")
    _w(root, "docs/guide/widgets.md", "<!-- domain: other -->\n\n# W\n")
    r = _run(root)
    assert r.returncode == 1
    assert "doppler.sig: has classes but no guide page filed" in r.stderr


def test_the_exemption_covers_it_and_a_written_guide_is_stale(
    tmp_path: Path,
) -> None:
    root = _fixture(tmp_path)
    (root / "docs/guide/widgets.md").unlink()
    # the class's own guide gap is a separate, older rule
    _w(root, "docs/.user-docs-exempt", "doppler.sig.Widget guide: old\n")
    r = _run(root)
    assert r.returncode == 1, r.stdout
    _w(root, GUIDES, "sig guide: predates the gate\n")
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr
    _w(root, "docs/guide/widgets.md", FM + "# Widgets\n\nUse `Widget`.\n")
    (root / "docs/.user-docs-exempt").unlink()
    r = _run(root)
    assert r.returncode == 1
    assert "'sig guide' is stale" in r.stderr


def test_the_guide_exemptions_only_shrink(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    (root / "docs/guide/widgets.md").unlink()
    _w(root, "scripts/gen_domains.py", "# the gate, present at base\n")
    _w(root, GUIDES, "# empty at base\n")
    _git(root, "init", "-q")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "base")
    _w(root, GUIDES, "sig guide: I would rather not\n")
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), "--base", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 1
    assert "'sig guide' is new; the list only shrinks" in r.stderr
