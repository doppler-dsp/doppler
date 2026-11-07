"""``changelog-assemble --lint``: a bad fragment is red on its own PR.

The assembler refuses a fragment that is not a ``- `` entry, or that carries a
``### `` heading, but it ran only at the release cut. Two prose fragments
reached main that way and stopped the v0.80.0 ``make release-pr`` halfway
(doppler#2896). ``--lint`` (``make lint-changelog-fragments``) applies the
assembler's own ``fragment_problem`` to every fragment in the tree.

The real script is copied into a scratch tree and run as a subprocess, so
these exercise the artifact rather than a re-implementation of it; the last
case runs it on the real tree.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "changelog-assemble.py"

GOOD = "- **A thing changed.** Why it matters, in one entry.\n"


def _scratch(tmp_path: Path, fragments: dict[str, str]) -> Path:
    """The real script in a throwaway root, beside the seeded fragments."""
    (tmp_path / "scripts").mkdir()
    copy = tmp_path / "scripts" / "changelog-assemble.py"
    copy.write_text(SCRIPT.read_text(encoding="utf-8"), encoding="utf-8")
    for rel, text in fragments.items():
        f = tmp_path / "changelog.d" / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text, encoding="utf-8")
    return copy


def _lint(script: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(script), "--lint"],
        capture_output=True,
        encoding="utf-8",
        check=False,
    )


def test_a_well_formed_fragment_passes(tmp_path: Path) -> None:
    r = _lint(_scratch(tmp_path, {"changed/ok.md": GOOD}))
    assert r.returncode == 0, r.stderr
    assert "1 fragment(s) well-formed" in r.stdout


def test_a_prose_fragment_fails(tmp_path: Path) -> None:
    # The shape both v0.80.0 offenders had: a sentence, not a bullet.
    script = _scratch(
        tmp_path,
        {
            "changed/ok.md": GOOD,
            "fixed/prose.md": "Fixed the thing that was broken.\n",
        },
    )
    r = _lint(script)
    assert r.returncode == 1
    # The em dash survives: the script writes UTF-8 on every OS.
    assert (
        "changelog.d/fixed/prose.md does not start with '- ' — a fragment"
        in r.stderr
    )
    assert "ok.md" not in r.stderr


def test_a_heading_line_fails(tmp_path: Path) -> None:
    script = _scratch(
        tmp_path, {"changed/heading.md": GOOD + "\n### Removed\n"}
    )
    r = _lint(script)
    assert r.returncode == 1
    assert "changelog.d/changed/heading.md line 3" in r.stderr


def test_every_bad_fragment_is_reported_not_just_the_first(
    tmp_path: Path,
) -> None:
    script = _scratch(
        tmp_path,
        {"added/a.md": "prose\n", "removed/b.md": GOOD + "### Added\n"},
    )
    r = _lint(script)
    assert r.returncode == 1
    assert "added/a.md" in r.stderr
    assert "removed/b.md" in r.stderr
    assert "2 fragment(s) the release cut would refuse" in r.stderr


def test_lint_mutates_nothing(tmp_path: Path) -> None:
    script = _scratch(tmp_path, {"changed/ok.md": GOOD})
    _lint(script)
    frag = tmp_path / "changelog.d" / "changed" / "ok.md"
    assert frag.read_text(encoding="utf-8") == GOOD
    assert not (tmp_path / "CHANGELOG.md").exists()


def test_the_real_tree_passes() -> None:
    r = _lint(SCRIPT)
    assert r.returncode == 0, r.stderr
