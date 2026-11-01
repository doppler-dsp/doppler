"""The user-docs gate: each piece of a class's user story is required.

``scripts/check_user_docs.py`` holds phase 9 of ``adding-algorithms.md``:
every public class has an API entry, a guide mention and an example, and
every example is a gallery script or says why not. ``DelayDoppler`` shipped
with none of the user-facing three while every docs check passed. These
cases run the real script on a synthetic tree and take each piece away in
turn; each must go red, and the ratchet must refuse a new exemption.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_user_docs.py"
EX = "src/doppler/examples"
EXEMPT = "docs/.user-docs-exempt"
NO_FIGURE = f"{EX}/.no-figure"


def _w(root: Path, rel: str, text: str) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)


def _fixture(root: Path) -> Path:
    """One module exporting a class and a record type, fully documented."""
    _w(
        root,
        "src/doppler/sig/__init__.py",
        '__all__ = ["Widget", "WidgetStats", "helper"]\n',
    )
    _w(
        root,
        "src/doppler/sig/sig.pyi",
        "class Widget: ...\nclass WidgetStats: ...\ndef helper(): ...\n",
    )
    _w(root, "just-makeit.toml", 'record_name = "WidgetStats"\n')
    _w(
        root,
        "docs/api/python-sig.md",
        "# sig\n\n::: doppler.sig.Widget\n\n::: doppler.sig.WidgetStats\n",
    )
    _w(root, "docs/guide/widgets.md", "# Widgets\n\nUse `Widget` to ...\n")
    _w(
        root,
        f"{EX}/widget_demo.py",
        "from doppler.sig import Widget\nw = Widget()\n",
    )
    return root


def _run(
    root: Path, *gallery: str, base: str = "HEAD"
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), "--base", base]
        + [f"{EX}/{g}" for g in gallery],
        capture_output=True,
        text=True,
        check=False,
    )


def test_a_complete_story_passes(tmp_path: Path) -> None:
    r = _run(_fixture(tmp_path), "widget_demo.py")
    assert r.returncode == 0, r.stdout + r.stderr


def test_no_guide_mention_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "docs/guide/widgets.md", "# Widgets\n\nNothing named here.\n")
    r = _run(root, "widget_demo.py")
    assert r.returncode == 1
    assert "doppler.sig.Widget: no guide" in r.stderr


def test_no_example_fails(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, f"{EX}/widget_demo.py", "print('hello')\n")
    r = _run(root, "widget_demo.py")
    assert r.returncode == 1
    assert "doppler.sig.Widget: no example" in r.stderr


def test_a_bare_api_mention_is_not_an_entry(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(
        root,
        "docs/api/python-sig.md",
        "# sig\n\n`Widget` is mentioned.\n\n::: doppler.sig.WidgetStats\n",
    )
    r = _run(root, "widget_demo.py")
    assert r.returncode == 1
    assert "doppler.sig.Widget: no api" in r.stderr


def test_a_heading_or_member_list_is_an_entry(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(
        root,
        "docs/api/python-sig.md",
        "# sig\n\n## Widget — the thing\n\n::: doppler.sig\n"
        "    options:\n      members:\n        - WidgetStats\n",
    )
    r = _run(root, "widget_demo.py")
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_record_type_owes_only_its_api_entry(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "docs/api/python-sig.md", "# sig\n\n::: doppler.sig.Widget\n")
    r = _run(root, "widget_demo.py")
    assert r.returncode == 1
    assert "doppler.sig.WidgetStats: no api" in r.stderr
    assert "WidgetStats: no guide" not in r.stderr
    assert "WidgetStats: no example" not in r.stderr


def test_an_example_needs_a_gallery_decision(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    r = _run(root)
    assert r.returncode == 1
    assert "widget_demo.py: no gallery" in r.stderr
    _w(root, NO_FIGURE, "widget_demo.py: prints a single number\n")
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr


def test_no_figure_refuses_a_bare_or_wrong_line(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, NO_FIGURE, "widget_demo.py:\n")
    assert "has no reason" in _run(root).stderr
    _w(root, NO_FIGURE, "widget_demo.py: why\nghost.py: gone\n")
    assert "ghost.py names no example" in _run(root).stderr
    _w(root, NO_FIGURE, "widget_demo.py: why\n")
    assert "is a gallery script" in _run(root, "widget_demo.py").stderr


def test_an_exemption_covers_a_gap_and_a_closed_one_is_stale(
    tmp_path: Path,
) -> None:
    root = _fixture(tmp_path)
    _w(root, "docs/guide/widgets.md", "# Widgets\n")
    _w(root, EXEMPT, "doppler.sig.Widget guide: predates the gate\n")
    r = _run(root, "widget_demo.py")
    assert r.returncode == 0, r.stdout + r.stderr
    _w(root, "docs/guide/widgets.md", "# Widgets\n\nUse `Widget`.\n")
    r = _run(root, "widget_demo.py")
    assert r.returncode == 1
    assert "is stale" in r.stderr


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=root,
        check=True,
        capture_output=True,
    )


def test_the_exempt_list_only_shrinks(tmp_path: Path) -> None:
    root = _fixture(tmp_path)
    _w(root, "scripts/check_user_docs.py", "# the gate, present at base\n")
    _w(root, EXEMPT, "# empty at base\n")
    _git(root, "init", "-q")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "base")
    _w(root, "docs/guide/widgets.md", "# Widgets\n")
    _w(root, EXEMPT, "doppler.sig.Widget guide: I would rather not\n")
    r = _run(root, "widget_demo.py")
    assert r.returncode == 1
    assert "is new; the list only shrinks" in r.stderr
