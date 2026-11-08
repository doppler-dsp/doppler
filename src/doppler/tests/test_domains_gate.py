"""The domain tree is gated: each of its rules goes red when broken.

``docs/domains.toml`` names the one grouping the docs use (a domain is a
Python module with a task title). ``scripts/gen_domains.py`` holds its rules
and ``scripts/check_user_docs.py`` runs them on ``make lint``. These cases run
the real script on a synthetic tree and break each rule in turn.
"""

from __future__ import annotations

import re
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._docs_fixture import domain_line
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_user_docs.py"
EX = "src/doppler/examples"
TOML = "docs/domains.toml"
GUIDES = "docs/.domain-guides-exempt"
FM = domain_line("sig", "About widgets.")
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


def test_a_family_index_and_an_archive_owe_no_domain(tmp_path: Path) -> None:
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


def test_a_nested_index_is_a_real_page_and_owes_a_domain(
    tmp_path: Path,
) -> None:
    root = _fixture(tmp_path)
    _w(root, "docs/guide/sub/index.md", "# Sub guide\n")
    r = _run(root)
    assert r.returncode == 1
    assert "docs/guide/sub/index.md: no domain" in r.stderr


def _gd():
    sys.path.insert(0, str(REPO / "scripts"))
    import gen_domains as gd

    return gd


def test_the_banner_holds_no_comment_terminator() -> None:
    """A `-->` inside the banner closes it early and leaks onto the page."""
    for rel, text in _gd().render(REPO).items():
        first = text.splitlines()[0]
        assert first.count("-->") == 1 and first.endswith("-->"), rel


def test_a_symbols_kind_is_read_from_its_own_module(tmp_path: Path) -> None:
    """The kind comes from the module's own sources, so a same-named `def`
    elsewhere (an example) cannot relabel a class, and a name with neither
    is a constant."""
    root = _fixture(tmp_path)
    _w(
        root,
        "src/doppler/sig/__init__.py",
        '__all__ = ["Widget", "make", "N"]\n',
    )
    _w(root, "src/doppler/sig/sig.pyi", "class Widget: ...\ndef make(): ...\n")
    _w(root, f"{EX}/widget_demo.py", "def Widget():\n    pass\n")
    sys.path.insert(0, str(REPO / "scripts"))
    from _pydocs import kinds

    assert kinds(str(root / "src" / "doppler")) == {
        "doppler.sig": {"Widget": "class", "make": "function", "N": "constant"}
    }


def test_a_cross_domain_class_is_found_from_both_sides() -> None:
    """The delay-Doppler guide is filed under `correlation` (#2752 step 4),
    but a delay-Doppler Correlator is an acquisition search: the
    acquisition guide links it."""
    out = _gd().render(REPO)
    assert "guide/delay-doppler.md" in out["docs/domains/acquire.md"]
    assert "guide/acquisition.md" in out["docs/domains/correlation.md"]


def _nav_entries() -> list[str]:
    """Every `.md` target in the WHOLE of mkdocs.yml's nav, in order."""
    text = (REPO / "mkdocs.yml").read_text(encoding="utf-8")
    nav = text[text.index("\nnav:") :]
    return re.findall(r":\s*([\w./-]+\.md)\s*$", nav, re.MULTILINE)


def test_every_filed_page_is_in_the_nav_exactly_once() -> None:
    """Counted over all of mkdocs.yml, hand-kept entries (Tools) included,
    so a duplicate or an orphan cannot hide in the part nobody generated."""
    gd = _gd()
    entries = _nav_entries()
    for _fam, path in gd.pages(REPO):
        rel = path.relative_to(REPO / "docs").as_posix()
        assert entries.count(rel) == 1, (rel, entries.count(rel))


def test_a_section_index_lists_what_the_nav_does() -> None:
    """Each generated index names the pages mkdocs.yml's nav gives that
    family, read back from the file, not from the generator."""
    gd = _gd()
    nav = set(_nav_entries())
    for rel, full in gd.index_blocks(REPO).items():
        fam = rel.split("/")[1]
        text = full[full.index(gd.IDX_START) : full.index(gd.IDX_END)]
        listed = {
            f"{fam}/{m}"
            for m in re.findall(r"\]\(([^)#]+\.md)\)", text)
            if "/" not in m or fam == "guide"
        }
        in_nav = {
            n
            for n in nav
            if n.startswith(f"{fam}/")
            and not gd.in_subsection(REPO, REPO / "docs" / n)
            and n != f"{fam}/index.md"
        }
        assert listed == in_nav, (rel, listed ^ in_nav)


def test_a_duplicated_marker_pair_is_refused() -> None:
    gd = _gd()
    text = (REPO / "mkdocs.yml").read_text(encoding="utf-8")
    block = gd.nav_blocks(REPO)["design-nav"]
    with pytest.raises(SystemExit):
        gd.apply_nav(text + block, "design-nav", block)


def test_a_page_without_a_summary_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "docs/guide/bare.md", domain_line("sig", None) + "# Bare\n")
    r = _run(root)
    assert r.returncode == 1
    assert "docs/guide/bare.md: no summary" in r.stderr


def test_a_tool_page_owes_no_summary(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    for n in ("index", "a"):
        _w(
            root,
            f"docs/guide/wfmgen/{n}.md",
            domain_line("sig", None) + "# A\n",
        )
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_summary_reaches_the_index_and_the_domain_page() -> None:
    gd = _gd()
    out = gd.render(REPO)
    line = "- [Delay-Doppler Search](delay-doppler.md) — " + gd.page_summary(
        REPO / "docs/guide/delay-doppler.md"
    )
    assert line in gd.index_blocks(REPO)["docs/guide/index.md"]
    # The delay-Doppler guide is the correlation domain's (#2752 step 4).
    assert "guide/delay-doppler.md) — " in out["docs/domains/correlation.md"]


@pytest.mark.parametrize(
    ("marker", "needle"),
    [
        ("<!-- domain: sig; summary: a ## Heading -->", "holds `##`"),
        ("<!-- domain: sig; summary: one\n two -->", "ONE line"),
        ("<!-- domain: sig; summary: one --> two -->", "ONE line"),
        ("<!-- domain: sig; summary: " + "x" * 301 + " -->", "keep it to"),
    ],
)
def test_a_summary_is_one_short_index_line(
    tmp_path: Path, marker: str, needle: str
) -> None:
    root = _fixture(tmp_path)
    _w(root, "docs/guide/bad.md", marker + "\n\n# Bad\n")
    r = _run(root)
    assert r.returncode == 1
    assert needle in r.stderr


def test_no_summary_in_the_tree_swallows_a_heading() -> None:
    gd = _gd()
    assert [x for x in gd.problems(REPO) if "summary" in x] == []
