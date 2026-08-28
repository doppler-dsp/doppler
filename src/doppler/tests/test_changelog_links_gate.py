"""The CHANGELOG comparison-link generator, and the release wiring around it.

`docs/dev/release.md` step 4 asked the releaser to hand-write a link line per
release and nothing checked it. 0.43.0, 0.43.1 and 0.43.2 all shipped without
one and there was no `[unreleased]:` definition at all (doppler#996). Those
four were backfilled by hand, which is not a fix -- and when this generator
first ran it found **seven more** nobody had noticed, plus one wrong link that
chained `v0.4.1...v0.5.0` straight past 0.4.6.

The cases below are the ones where a WEAKER implementation still looks right:
a generator that reshuffles eighty-one correct lines to add eight, one that
writes an empty block when it can find no headings, and one that treats the
first release -- which has no predecessor to compare against -- like the rest.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "gen_changelog_links.py"
BASE = "https://github.com/doppler-dsp/doppler"


def _write(tmp_path: Path, body: str) -> Path:
    p = tmp_path / "CHANGELOG.md"
    p.write_text(body, encoding="utf-8")
    return p


def _run(changelog: Path, *args: str):
    """Run the generator against a seeded CHANGELOG via a scratch repo root.

    The script resolves CHANGELOG.md from its own location, so the fixture
    lays out a scripts/ dir beside it and runs the real file from there --
    the artifact, not a re-implementation of it.
    """
    scratch_scripts = changelog.parent / "scripts"
    scratch_scripts.mkdir(exist_ok=True)
    copy = scratch_scripts / "gen_changelog_links.py"
    copy.write_text(SCRIPT.read_text(encoding="utf-8"), encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(copy), *args],
        capture_output=True,
        text=True,
    )


THREE = f"""# Changelog

## [Unreleased]

## [0.3.0] — 2026-01-03

## [0.2.0] — 2026-01-02

## [0.1.0] — 2026-01-01

[0.1.0]: {BASE}/releases/tag/v0.1.0
[0.2.0]: {BASE}/compare/v0.1.0...v0.2.0
[0.3.0]: {BASE}/compare/v0.2.0...v0.3.0
[unreleased]: {BASE}/compare/v0.3.0...HEAD
"""


def test_a_correct_block_is_left_alone(tmp_path: Path) -> None:
    """The check passes on a file that is already right."""
    c = _write(tmp_path, THREE)
    r = _run(c, "--check")
    assert r.returncode == 0, r.stdout + r.stderr
    assert "4 definition(s)" in r.stdout


def test_a_missing_definition_fails_and_names_it(tmp_path: Path) -> None:
    """The defect the gate exists for: a heading with no link."""
    c = _write(
        tmp_path,
        THREE.replace(f"[0.2.0]: {BASE}/compare/v0.1.0...v0.2.0\n", ""),
    )
    r = _run(c, "--check")
    assert r.returncode == 1
    assert "[0.2.0]" in r.stderr
    # and it must say what the reader would SEE, not just that a line is gone
    assert "literal" in r.stderr


def test_the_first_release_points_at_its_tag(tmp_path: Path) -> None:
    """The oldest release has no predecessor, so `compare/` is wrong for it.

    A generator that treated every version alike would emit
    `compare/v...v0.1.0` with an empty left side -- a link that resolves to
    nothing on GitHub, and the one case a uniform rule gets wrong.
    """
    c = _write(tmp_path, THREE)
    _run(c, "--write")
    assert f"[0.1.0]: {BASE}/releases/tag/v0.1.0" in c.read_text()


def test_a_new_release_chains_to_the_previous_one(tmp_path: Path) -> None:
    """Adding a heading is the whole of the work -- the link is derived."""
    body = THREE.replace(
        "## [Unreleased]", "## [Unreleased]\n\n## [0.4.0] — 2026-01-04"
    )
    c = _write(tmp_path, body)
    assert _run(c, "--check").returncode == 1
    _run(c, "--write")
    text = c.read_text()
    assert f"[0.4.0]: {BASE}/compare/v0.3.0...v0.4.0" in text
    # and unreleased now compares against the NEW newest, not the old one
    assert f"[unreleased]: {BASE}/compare/v0.4.0...HEAD" in text
    assert _run(c, "--check").returncode == 0


def test_an_intermediate_release_is_not_chained_past(tmp_path: Path) -> None:
    """The real defect found on the first run, in miniature.

    `[0.5.0]` linked `v0.4.1...v0.5.0` while 0.4.6 sat between them -- because
    0.4.6 had no link of its own, whoever wrote it chained around the gap. A
    generator reading the headings cannot make that mistake.
    """
    body = THREE.replace(
        "## [0.2.0] — 2026-01-02",
        "## [0.2.5] — 2026-01-02\n\n## [0.2.0] — 2026-01-02",
    )
    c = _write(tmp_path, body)
    _run(c, "--write")
    text = c.read_text()
    assert f"[0.2.5]: {BASE}/compare/v0.2.0...v0.2.5" in text
    assert f"[0.3.0]: {BASE}/compare/v0.2.5...v0.3.0" in text
    assert f"{BASE}/compare/v0.2.0...v0.3.0" not in text


def test_no_headings_refuses_rather_than_emptying_the_block(
    tmp_path: Path,
) -> None:
    """Nothing to derive from means nothing was derived.

    Without this the generator would cheerfully write an empty block over a
    correct one the moment the heading format changed -- destroying the thing
    it exists to maintain, and exiting 0 while doing it.
    """
    c = _write(tmp_path, "# Changelog\n\nno headings here\n")
    r = _run(c, "--write")
    assert r.returncode == 1
    assert "no `## [version]` headings" in r.stderr
    assert "no headings here" in c.read_text()


def test_the_real_changelog_is_consistent() -> None:
    """The gate, applied to doppler's own CHANGELOG -- the artifact."""
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--check"],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr


def test_tag_release_requires_the_fragments_to_be_assembled() -> None:
    """`changelog-assembled-check` must run before the irreversible step.

    It used to be named in `LOCAL_TARGETS` -- the .PHONY/help list -- and in
    nothing else: not `GATES_DEPS`, not `lint`, no CI job. This asserts the
    dependency exists in make's own graph rather than grepping the Makefile
    for a line, because a line that make does not act on is exactly the
    failure being fixed.
    """
    r = subprocess.run(
        ["make", "-n", "tag-release", "VERSION=0.0.0"],
        cwd=REPO,
        capture_output=True,
        text=True,
    )
    combined = r.stdout + r.stderr
    assert "changelog-assemble.py --check" in combined, combined[-2000:]
