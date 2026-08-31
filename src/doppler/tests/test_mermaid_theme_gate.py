"""The mermaid theme gate, exercised over seeded diagram sources.

`scripts/check_mermaid_theme.py` guards a defect that shipped in three
diagrams at once and was only ever visible to a reader who did not touch the
palette toggle. The docs default to `slate` (mkdocs.yml lists it first), and
zensical drives mermaid from the palette's own CSS variables — a node's text
colour resolves to `--md-code-fg-color`, near-white in the dark theme. A
diagram that pinned `fill:#ede7f6` therefore pinned one half of a contrast
pair while the other half kept moving: near-white on near-white.

A gate proven only against a tree that happens to pass is a gate nobody has
seen fail, so the script takes `--root` and this file drives it over
markdown written here: each pinned property must be caught, and the forms
that are legitimately theme-following must clear it.

The `stroke-dasharray` case is not decoration. It is the false positive that
would make the gate unusable — a naive search for "color" inside a style line
matches nothing in it, but a naive search for the *substring* would, and an
outline is exactly what the fix tells authors to reach for instead. A gate
that rejected the remedy it recommends would be worse than no gate.
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
SCRIPT = REPO / "scripts" / "check_mermaid_theme.py"


def _check(tmp_path: Path, page: str) -> int:
    """Seed a one-page docs tree with `page` and return the gate's code."""
    docs = tmp_path / "docs"
    docs.mkdir(exist_ok=True)
    (docs / "seeded.md").write_text(page, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    ).returncode


def _diagram(style: str) -> str:
    return (
        "# seeded\n\n```mermaid\nflowchart LR\n"
        '    a["one"] --> b["two"]\n'
        f"    {style}\n"
        "    class a k;\n```\n"
    )


@pytest.mark.parametrize(
    "style",
    [
        "classDef k fill:#ede7f6,stroke:#5e35b1,color:#000;",
        "classDef k fill:#ede7f6;",
        "classDef k color:#000;",
        "classDef k fill:white;",
        "style a fill:#fff;",
        "linkStyle 0 color:#333;",
    ],
)
def test_a_pinned_colour_is_caught(tmp_path: Path, style: str) -> None:
    """Every way of pinning a fill or a text colour fails the gate."""
    assert _check(tmp_path, _diagram(style)) == 1


@pytest.mark.parametrize(
    "style",
    [
        "classDef k stroke-width:3px;",
        "classDef k stroke:#16a34a,stroke-width:3px;",
        # The remedy the gate itself recommends must not be rejected.
        "classDef k stroke-width:3px,stroke-dasharray:4 2;",
        # Values that follow the theme rather than fixing it.
        "classDef k fill:none;",
        "classDef k fill:transparent;",
        "classDef k color:var(--md-code-fg-color);",
    ],
)
def test_theme_following_styling_passes(tmp_path: Path, style: str) -> None:
    """Outlines and theme-derived values are the intended way to style."""
    assert _check(tmp_path, _diagram(style)) == 0


def test_only_mermaid_fences_are_scanned(tmp_path: Path) -> None:
    """A `fill:` in prose or in another language's fence is not a diagram.

    Without this the gate would report every CSS snippet in the docs, which
    is how a checker becomes something people route around.
    """
    page = (
        "# seeded\n\n"
        "```css\n.node rect { fill:#ede7f6; color:#000; }\n```\n\n"
        "```mermaid\nflowchart LR\n"
        '    a["one"] --> b["two"]\n'
        "    classDef k stroke-width:3px;\n"
        "    class a k;\n```\n"
    )
    assert _check(tmp_path, page) == 0


def test_no_diagrams_at_all_fails_closed(tmp_path: Path) -> None:
    """Zero diagrams is not a pass — absent output is not a passing check.

    If the fence syntax or the docs directory moved, the gate must say so
    rather than print OK over a tree it never looked at.
    """
    (tmp_path / "docs").mkdir()
    (tmp_path / "docs" / "seeded.md").write_text("# no diagrams\n")
    assert (
        subprocess.run(
            [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
            capture_output=True,
            text=True,
        ).returncode
        == 1
    )


def test_the_report_names_the_page_and_the_property(tmp_path: Path) -> None:
    """A failure has to be actionable: which file, which line, which knob."""
    docs = tmp_path / "docs"
    docs.mkdir()
    (docs / "seeded.md").write_text(
        _diagram("classDef k fill:#ede7f6,color:#000;"), encoding="utf-8"
    )
    proc = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )
    assert "docs/seeded.md:6" in proc.stderr
    assert "(fill)" in proc.stderr
    assert "(color)" in proc.stderr


def test_the_real_repository_passes() -> None:
    """doppler's own docs are clean, which is what the fix left behind."""
    proc = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert proc.returncode == 0, proc.stderr
