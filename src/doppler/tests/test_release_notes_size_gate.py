"""The release-notes size gate, exercised over seeded changelogs.

`scripts/check_release_notes_size.py` guards a failure that lands in the worst
possible place. `release.yml`'s `github-release` job lists `publish-python` in
its `needs`, so a body GitHub refuses fails *after* the version is on PyPI —
and PyPI will not accept a re-upload of a version, so the ordinary "rerun the
release" recovery is gone. Measured on 2026-08-22, the projected body stood at
410,891 characters against a 125,000 cap.

The gate carries a second job in the same measurement: deferring a release is
exactly what makes the body grow, so the size *is* the cadence signal and warns
before it blocks. That is why there is no separate "days since the last tag"
rule to drift out of step with this one.

A gate proven only against the tree it ships with is a gate nobody has watched
fail — and here the live tree fails, so the case actually needing proof is the
green one. All three bands are driven over content written here.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_release_notes_size.py"

# Kept in step with the script; asserted below so a change there fails here
# rather than silently loosening the gate.
BUDGET = 100_000


def _run(
    tmp_path: Path,
    entry_chars: int,
    n_fragment_chars: int = 0,
    slot: str = "a",
    highlights: str = "",
):
    """Seed a changelog of a chosen size and return the gate's result.

    `slot` keeps two calls in one test from colliding in the same tmp_path.
    """
    tmp_path = tmp_path / slot
    tmp_path.mkdir(exist_ok=True)
    changelog = tmp_path / "CHANGELOG.md"
    body = "- **An entry.** " + ("x" * entry_chars) + "\n"
    hl = f"### Highlights\n\n{highlights}\n" if highlights else ""
    changelog.write_text(
        "# Changelog\n\n## [Unreleased]\n\n"
        + hl
        + "\n### Added\n\n"
        + body
        + "\n## [0.1.0] — 2026-01-01\n\n### Added\n\n- **Old.** y\n",
        encoding="utf-8",
    )
    frags = tmp_path / "changelog.d"
    (frags / "added").mkdir(parents=True)
    if n_fragment_chars:
        (frags / "added" / "f.md").write_text(
            "- **Fragment.** " + ("z" * n_fragment_chars) + "\n",
            encoding="utf-8",
        )
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--changelog",
            str(changelog),
            "--fragments",
            str(frags),
        ],
        capture_output=True,
        text=True,
    )


def test_budget_matches_the_script() -> None:
    """The number this file reasons about is the number the gate enforces."""
    src = SCRIPT.read_text(encoding="utf-8")
    assert "GITHUB_BODY_LIMIT = 125_000" in src
    assert "PREAMBLE_RESERVE = 1_000" in src
    assert "GENERATED_NOTES_RESERVE = 24_000" in src


def test_a_small_changelog_passes(tmp_path: Path) -> None:
    """The green case — the one the live tree cannot demonstrate."""
    r = _run(tmp_path, 1_000)
    assert r.returncode == 0, r.stdout
    assert "OK" in r.stdout


def test_over_budget_fails(tmp_path: Path) -> None:
    r = _run(tmp_path, BUDGET + 5_000)
    assert r.returncode == 1, r.stdout
    assert "FAIL" in r.stdout
    # The remedy has to be in the message: this fires on a release PR, where
    # "too big" without "summarise or cut a release" is not actionable.
    assert "Highlights" in r.stdout


def test_half_budget_warns_without_failing(tmp_path: Path) -> None:
    """The cadence half: pressure well before the wall, but not a blocker."""
    r = _run(tmp_path, int(BUDGET * 0.6))
    assert r.returncode == 0, r.stdout
    assert "a release is due" in r.stdout


def test_unassembled_fragments_count(tmp_path: Path) -> None:
    """`changelog.d/` is promoted verbatim at release time, so it counts.

    A gate reading only CHANGELOG.md would stay green right up until the
    release commit assembled the fragments — i.e. it would go red for the
    first time on the release PR, having been useless for every PR that
    caused the problem.
    """
    under = _run(tmp_path, 40_000, slot="under")
    assert under.returncode == 0, under.stdout

    over = _run(tmp_path, 40_000, n_fragment_chars=BUDGET, slot="over")
    assert over.returncode == 1, over.stdout


def test_rendered_body_mode_fails_when_too_large(tmp_path: Path) -> None:
    """`--body-file` is what release-notes.sh calls on the real body."""
    body = tmp_path / "notes.md"
    body.write_text("x" * 120_000, encoding="utf-8")
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--body-file", str(body)],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 1, r.stdout

    body.write_text("x" * 1_000, encoding="utf-8")
    ok = subprocess.run(
        [sys.executable, str(SCRIPT), "--body-file", str(body)],
        capture_output=True,
        text=True,
    )
    assert ok.returncode == 0, ok.stdout


def test_highlights_rescue_an_over_budget_section(tmp_path: Path) -> None:
    """The escape the failure message tells you to take must actually work.

    A message recommending a remedy the gate then rejects is worse than no
    message: it reads as authoritative and sends you to do the wrong thing.
    """
    without = _run(tmp_path, BUDGET + 5_000, slot="without")
    assert without.returncode == 1, without.stdout

    with_hl = _run(
        tmp_path,
        BUDGET + 5_000,
        slot="with",
        highlights="- **The headline.** A short summary of the release.\n",
    )
    assert with_hl.returncode == 0, with_hl.stdout
    # ...and it still says a release is due, because the accumulation is real
    # even though it is now publishable.
    assert "a release is due" in with_hl.stdout
    assert "via ### Highlights" in with_hl.stdout


def test_an_over_long_highlights_block_still_fails(tmp_path: Path) -> None:
    """Highlights is an escape from the cap, not a way to disable it."""
    r = _run(
        tmp_path,
        BUDGET + 5_000,
        slot="fat",
        highlights="- **Too much.** " + ("q" * (BUDGET + 1_000)),
    )
    assert r.returncode == 1, r.stdout
    assert "itself too long" in r.stdout


def test_highlights_is_ignored_when_the_section_already_fits(
    tmp_path: Path,
) -> None:
    """Below the budget the full section is better notes, so it wins."""
    r = _run(tmp_path, 1_000, slot="small", highlights="- **Tiny.** x\n")
    assert r.returncode == 0, r.stdout
    assert "via ### Highlights" not in r.stdout


def test_highlights_bullets_are_not_counted_as_entries(tmp_path: Path) -> None:
    """The remedy must not inflate the number that reports the problem.

    Highlights summarises the release, so counting its bullets alongside the
    real entries makes the count climb every time the summary is improved —
    and the figure is what a reader uses to judge whether a release is due.
    """
    hl = "".join(f"- **Point {i}.** x\n" for i in range(10))
    r = _run(tmp_path, 1_000, slot="counted", highlights=hl)
    assert r.returncode == 0, r.stdout
    # one seeded entry, not eleven
    assert "1 entry" in r.stdout, r.stdout


# --- release-notes.sh, the renderer the gate protects ------------------------

NOTES = REPO / "scripts" / "release-notes.sh"


def _notes(tmp_path: Path, section: str, name: str = "CHANGELOG.md"):
    """Render the release body for 0.43.0 from a seeded changelog."""
    cl = tmp_path / name
    cl.write_text(
        f"# Changelog\n\n## [0.43.0] — 2026-08-22\n\n{section}\n",
        encoding="utf-8",
    )
    return subprocess.run(
        ["bash", str(NOTES), "0.43.0"],
        capture_output=True,
        text=True,
        env={"PATH": "/usr/bin:/bin:/usr/local/bin", "CHANGELOG": str(cl)},
    )


def _bulk(n: int = 400) -> str:
    return "### Added\n\n" + "".join(
        f"- **Entry {i}.** {'x' * 300}\n\n" for i in range(n)
    )


HL = "### Highlights\n\n- **The headline.** A short summary.\n\n"


def test_a_small_section_is_published_in_full(tmp_path: Path) -> None:
    r = _notes(tmp_path, "### Added\n\n- **Small.** A short entry.\n")
    assert r.returncode == 0, r.stderr
    assert "Small" in r.stdout
    assert "This release is large" not in r.stdout


def test_an_oversized_section_falls_back_to_highlights(tmp_path: Path) -> None:
    r = _notes(tmp_path, HL + _bulk())
    assert r.returncode == 0, r.stderr
    assert "The headline" in r.stdout
    assert "This release is large" in r.stdout
    assert "Entry 1." not in r.stdout  # the full entries stay behind
    assert len(r.stdout) < 100_000


def test_an_oversized_section_without_highlights_refuses(
    tmp_path: Path,
) -> None:
    """Fail closed, before the tag — not after PyPI has the version."""
    r = _notes(tmp_path, _bulk())
    assert r.returncode == 1
    assert "no ### Highlights" in r.stderr


def test_extraction_does_not_die_of_sigpipe(tmp_path: Path) -> None:
    """Regression: exit 141 on a large section, and an EMPTY body.

    The Highlights extraction exits at the next `### ` heading. Fed by a pipe,
    the writer then takes SIGPIPE, and `set -o pipefail` turns that into a
    failed release — one that produced a zero-byte body while every visible
    step looked fine. A here-string has no such writer. This asserts the exit
    code specifically, because the symptom was indistinguishable from an
    ordinary failure until the 141 was noticed.
    """
    r = _notes(tmp_path, HL + _bulk(600))
    assert r.returncode == 0, f"exit {r.returncode}: {r.stderr}"
    assert r.returncode != 141
    assert r.stdout.strip(), "rendered an empty body"
