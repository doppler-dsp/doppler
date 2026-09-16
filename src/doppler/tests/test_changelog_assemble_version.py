"""`changelog-assemble --version`: the release's step 4, done mechanically.

`docs/dev/release.md` asked the releaser to rename `## [Unreleased]` to
`## [X.Y.Z] - <date>` and open a fresh one **by hand**, immediately after a
command that had already edited the same file. A hand step next to an
automated one is the half that rots: the sibling link-writing step was also
hand-written until 0.43.0, 0.43.1 and 0.43.2 shipped without comparison links
and nothing noticed (doppler#996).

The cases below are the ones where a weaker implementation still looks right:
one that moves the entries instead of renaming the heading above them, one
that happily opens a SECOND section for a version already cut, and one that
lets `--check` -- whose whole contract is "mutates nothing" -- write.

The real script is copied into a scratch tree and run as a subprocess, so
these exercise the artifact rather than a re-implementation of it.
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

HEAD = "# Changelog\n\n## [Unreleased]\n\n- an entry already here\n"


def _scratch(tmp_path: Path, body: str = HEAD) -> Path:
    """A throwaway repo root: the real script, beside a seeded CHANGELOG."""
    (tmp_path / "scripts").mkdir(exist_ok=True)
    copy = tmp_path / "scripts" / "changelog-assemble.py"
    copy.write_text(SCRIPT.read_text(encoding="utf-8"), encoding="utf-8")
    (tmp_path / "CHANGELOG.md").write_text(body, encoding="utf-8")
    return copy


def _fragment(tmp_path: Path, section: str, name: str, text: str) -> None:
    d = tmp_path / "changelog.d" / section
    d.mkdir(parents=True, exist_ok=True)
    (d / name).write_text(text, encoding="utf-8")


def _run(script: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(script), *args],
        capture_output=True,
        text=True,
        check=False,
    )


def test_renames_and_opens_a_fresh_unreleased(tmp_path: Path) -> None:
    """The heading above the entries changes; the entries do not move."""
    script = _scratch(tmp_path)
    r = _run(script, "--version", "1.2.3")
    assert r.returncode == 0, r.stderr
    out = (tmp_path / "CHANGELOG.md").read_text()
    assert "## [Unreleased]" in out
    assert "## [1.2.3] - " in out
    # The fresh Unreleased sits ABOVE the cut release.
    assert out.index("## [Unreleased]") < out.index("## [1.2.3]")
    # And the pre-existing entry became that release's body, in place.
    assert out.index("## [1.2.3]") < out.index("- an entry already here")


def test_promotes_and_renames_in_one_run(tmp_path: Path) -> None:
    """A fragment lands under the version it is being released in.

    Promotion writes into `[Unreleased]` and the rename happens after, so a
    fragment must end up under `[X.Y.Z]` -- not stranded above it in the
    fresh section, which is what an order swap would produce.
    """
    script = _scratch(tmp_path)
    _fragment(tmp_path, "added", "thing.md", "- **A thing** landed.\n")
    r = _run(script, "--version", "9.9.9")
    assert r.returncode == 0, r.stderr
    out = (tmp_path / "CHANGELOG.md").read_text()
    assert out.index("## [9.9.9]") < out.index("**A thing**")
    assert not (tmp_path / "changelog.d" / "added" / "thing.md").exists()


def test_a_version_already_cut_is_refused(tmp_path: Path) -> None:
    """Re-running must not open a second section for the same release."""
    script = _scratch(tmp_path)
    assert _run(script, "--version", "1.2.3").returncode == 0
    before = (tmp_path / "CHANGELOG.md").read_text()
    r = _run(script, "--version", "1.2.3")
    assert r.returncode != 0
    assert "already has a" in r.stderr
    assert (tmp_path / "CHANGELOG.md").read_text() == before, (
        "a refused run must leave the file byte-identical"
    )


def test_a_prerelease_is_refused(tmp_path: Path) -> None:
    """Same answer bump-version gives, for the same reason."""
    script = _scratch(tmp_path)
    r = _run(script, "--version", "1.2.3rc1")
    assert r.returncode != 0
    assert "not X.Y.Z" in r.stderr


def test_check_and_version_together_are_refused(tmp_path: Path) -> None:
    """`--check` mutates nothing; accepting both would break that."""
    script = _scratch(tmp_path)
    r = _run(script, "--check", "--version", "1.2.3")
    assert r.returncode != 0
    assert "opposites" in r.stderr
    assert (tmp_path / "CHANGELOG.md").read_text() == HEAD


def test_renames_even_with_no_fragments(tmp_path: Path) -> None:
    """The rename is not conditional on there being anything to promote."""
    script = _scratch(tmp_path)
    r = _run(script, "--version", "0.1.0")
    assert r.returncode == 0, r.stderr
    assert "## [0.1.0] - " in (tmp_path / "CHANGELOG.md").read_text()
