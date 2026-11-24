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

import os
import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._platform import skip_without_posix_shell
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

    import pytest

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_release_notes_size.py"

# Kept in step with the script; asserted below so a change there fails here
# rather than silently loosening the gate.
BUDGET = 100_000


def _assert_refused(r, *phrases: str) -> None:
    """A refusal is exit 1, the gate's own words, and no crash.

    A non-zero exit alone is not a refusal: an uncaught exception also exits 1
    (#3335, where a Windows crash satisfied an expected red). So every test
    that expects a refusal names the gate's sentence and rules out a traceback.
    """
    out = r.stdout + r.stderr
    assert r.returncode == 1, out
    assert "Traceback" not in out, out
    for phrase in phrases:
        assert phrase in out, out


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
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK" in r.stdout + r.stderr


def test_over_budget_fails(tmp_path: Path) -> None:
    r = _run(tmp_path, BUDGET + 5_000)
    # The remedy has to be in the message: this fires on a release PR, where
    # "too big" without "summarise or cut a release" is not actionable.
    _assert_refused(r, "FAIL", "Highlights")


def test_half_budget_warns_without_failing(tmp_path: Path) -> None:
    """The cadence half: pressure well before the wall, but not a blocker."""
    r = _run(tmp_path, int(BUDGET * 0.6))
    assert r.returncode == 0, r.stdout + r.stderr
    assert "a release is due" in r.stdout + r.stderr


def test_unassembled_fragments_count(tmp_path: Path) -> None:
    """`changelog.d/` is promoted verbatim at release time, so it counts.

    A gate reading only CHANGELOG.md would stay green right up until the
    release commit assembled the fragments — i.e. it would go red for the
    first time on the release PR, having been useless for every PR that
    caused the problem.
    """
    under = _run(tmp_path, 40_000, slot="under")
    assert under.returncode == 0, under.stdout + under.stderr

    over = _run(tmp_path, 40_000, n_fragment_chars=BUDGET, slot="over")
    _assert_refused(over, "FAIL")


def test_rendered_body_mode_fails_when_too_large(tmp_path: Path) -> None:
    """`--body-file` is what release-notes.sh calls on the real body."""
    body = tmp_path / "notes.md"
    body.write_text("x" * 120_000, encoding="utf-8")
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--body-file", str(body)],
        capture_output=True,
        text=True,
    )
    _assert_refused(r, "FAIL")

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
    _assert_refused(without, "FAIL")

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
    _assert_refused(r, "itself too long")


def test_highlights_is_ignored_when_the_section_already_fits(
    tmp_path: Path,
) -> None:
    """Below the budget the full section is better notes, so it wins."""
    r = _run(tmp_path, 1_000, slot="small", highlights="- **Tiny.** x\n")
    assert r.returncode == 0, r.stdout + r.stderr
    assert "via ### Highlights" not in r.stdout + r.stderr


def test_highlights_bullets_are_not_counted_as_entries(tmp_path: Path) -> None:
    """The remedy must not inflate the number that reports the problem.

    Highlights summarises the release, so counting its bullets alongside the
    real entries makes the count climb every time the summary is improved —
    and the figure is what a reader uses to judge whether a release is due.
    """
    hl = "".join(f"- **Point {i}.** x\n" for i in range(10))
    r = _run(tmp_path, 1_000, slot="counted", highlights=hl)
    assert r.returncode == 0, r.stdout + r.stderr
    # one seeded entry, not eleven
    assert "1 entry" in r.stdout + r.stderr, r.stdout + r.stderr


# --- release-notes.sh, the renderer the gate protects ------------------------

NOTES = REPO / "scripts" / "release-notes.sh"


def _notes(tmp_path: Path, section: str, name: str = "CHANGELOG.md"):
    """Render the release body for 0.43.0 from a seeded changelog."""
    skip_without_posix_shell("scripts/release-notes.sh")
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
    assert "Small" in r.stdout + r.stderr
    assert "This release is large" not in r.stdout + r.stderr


def test_an_oversized_section_falls_back_to_highlights(tmp_path: Path) -> None:
    r = _notes(tmp_path, HL + _bulk())
    assert r.returncode == 0, r.stderr
    assert "The headline" in r.stdout + r.stderr
    assert "This release is large" in r.stdout + r.stderr
    assert (
        "Entry 1." not in r.stdout + r.stderr
    )  # the full entries stay behind
    assert len(r.stdout + r.stderr) < 100_000


def test_an_oversized_section_without_highlights_refuses(
    tmp_path: Path,
) -> None:
    """Fail closed, before the tag — not after PyPI has the version."""
    r = _notes(tmp_path, _bulk())
    _assert_refused(r, "no ### Highlights")


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
    assert r.stdout + r.stderr.strip(), "rendered an empty body"


# The release body's relative links (#3319). The fixture is the link text
# v0.86.0's published body carried: eight links, three targets, each a
# repo-relative path that 404'd under /releases/tag/. Kept verbatim so the
# rewrite is judged on the
# links that broke, not on links someone imagined.
V086_LINKS = [
    "[§4](docs/design/async-dsss-receiver.md#4-the-carrier-loop)",
    "[§2](docs/design/async-dsss-receiver.md#2-the-lock)",
    "[§1](docs/design/async-dsss-receiver.md)",
    "[§7](docs/design/async-dsss-receiver.md)",
    "[§5](docs/design/async-dsss-receiver.md)",
    "[§6](docs/design/async-dsss-receiver.md)",
    "[the C reference](docs/c-api/async__dsss__receiver__core_8h.md)",
    "[the spec demo](docs/gallery/async-dsss-receiver-spec.md)",
]
BLOB = "https://github.com/doppler-dsp/doppler/blob/v0.86.0/"


def _check_module():
    """The rewrite and its check, loaded the way the script loads them."""
    import importlib.util

    spec = importlib.util.spec_from_file_location("crns", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def test_the_v086_links_resolve_to_the_tag() -> None:
    """Each broken link now names the blob URL at the tag, anchor kept."""
    mod = _check_module()
    body = "\n".join(V086_LINKS) + "\n"
    out = mod.absolutize(body, BLOB)
    assert mod.unresolved_links(out) == []
    assert (
        f"]({BLOB}docs/design/async-dsss-receiver.md#4-the-carrier-loop)"
        in out
    )
    assert f"]({BLOB}docs/gallery/async-dsss-receiver-spec.md)" in out
    assert out.count(BLOB) == len(V086_LINKS)


def test_the_relative_links_are_refused_before_the_rewrite() -> None:
    """The check names what the rewrite left, so the gate can say so."""
    mod = _check_module()
    assert mod.unresolved_links("[a](docs/x.md) [b](#anchor)") == ["docs/x.md"]


def test_an_escaping_link_survives_the_rewrite_and_is_named() -> None:
    """`../` has no place on the tag: left for the gate, not guessed."""
    mod = _check_module()
    out = mod.absolutize("[x](../secret.md)", BLOB)
    assert out == "[x](../secret.md)"
    assert mod.unresolved_links(out) == ["../secret.md"]


def test_a_dot_slash_and_a_root_path_resolve() -> None:
    mod = _check_module()
    out = mod.absolutize("[a](./docs/a.md) [b](/docs/b.md)", BLOB)
    assert f"[a]({BLOB}docs/a.md)" in out
    assert f"[b]({BLOB}docs/b.md)" in out


def test_a_fragment_with_an_escaping_link_fails_the_lint_gate(
    tmp_path: Path,
) -> None:
    """The PR that adds the link goes red at lint, not at release (#3319)."""
    r = _run(tmp_path, 1_000, slot="escape", n_fragment_chars=0)
    assert r.returncode == 0, r.stdout + r.stderr
    bad = tmp_path / "escape" / "changelog.d" / "added" / "bad.md"
    bad.write_text("- **Bad.** see [x](../outside.md)\n", encoding="utf-8")
    r = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--changelog",
            str(tmp_path / "escape" / "CHANGELOG.md"),
            "--fragments",
            str(tmp_path / "escape" / "changelog.d"),
        ],
        capture_output=True,
        text=True,
    )
    _assert_refused(r, "neither absolute", "../outside.md")


def test_the_body_check_fails_on_a_relative_link(tmp_path: Path) -> None:
    """`--body-file` is the release-pr home: a relative link fails there."""
    body = tmp_path / "notes.md"
    body.write_text(
        "## What's in 1.0\n\nSee [§1](docs/design/async-dsss-receiver.md).\n",
        encoding="utf-8",
    )
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--body-file", str(body)],
        capture_output=True,
        text=True,
    )
    _assert_refused(
        r, "neither absolute", "docs/design/async-dsss-receiver.md"
    )


def test_the_script_publishes_absolute_links_at_the_tag(
    tmp_path: Path,
) -> None:
    """End to end: release-notes.sh renders a link at the tag's blob URL."""
    skip_without_posix_shell("scripts/release-notes.sh")
    changelog = tmp_path / "CHANGELOG.md"
    changelog.write_text(
        "# Changelog\n\n## [9.9.9] — 2026-01-01\n\n### Added\n\n"
        "- **Feature.** See [§1](docs/design/async-dsss-receiver.md#one).\n",
        encoding="utf-8",
    )
    script = REPO / "scripts" / "release-notes.sh"
    r = subprocess.run(
        ["bash", str(script), "9.9.9", "doppler-dsp/doppler"],
        capture_output=True,
        text=True,
        env={**__import__("os").environ, "CHANGELOG": str(changelog)},
    )
    assert r.returncode == 0, r.stderr
    assert (
        "(https://github.com/doppler-dsp/doppler/blob/v9.9.9/"
        "docs/design/async-dsss-receiver.md#one)" in r.stdout
    )
    assert "](docs/" not in r.stdout + r.stderr


def test_a_fragment_relative_link_is_rewritten_so_the_lint_gate_passes(
    tmp_path: Path,
) -> None:
    """A fragment's `docs/…` link is written as the changelog writes it, and
    the lint gate passes it, since the rendered form is absolute (#3319).
    Removing
    the rewrite turns this red, which is the sabotage the gate is proven by."""
    r = _run(tmp_path, 1_000, slot="rel", n_fragment_chars=0)
    assert r.returncode == 0, r.stdout + r.stderr
    frag = tmp_path / "rel" / "changelog.d" / "added" / "ok.md"
    frag.write_text(
        "- **Linked.** see [§1](docs/design/async-dsss-receiver.md#one)\n",
        encoding="utf-8",
    )
    r = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--changelog",
            str(tmp_path / "rel" / "CHANGELOG.md"),
            "--fragments",
            str(tmp_path / "rel" / "changelog.d"),
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_path_on_another_drive_is_named_not_raised(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Windows' relpath raises across drives: the gate still names the file.

    The raise came AFTER the OK line had printed, so the gate exited 1 with the
    reason on stderr and no FAIL line (#3335). Simulated here, since the box is
    one drive.
    """
    mod = _check_module()
    frags = tmp_path / "changelog.d" / "added"
    frags.mkdir(parents=True)
    long_entry = "- **Long.** headline\n" + "  more\n" * 12
    (frags / "long.md").write_text(long_entry, encoding="utf-8")

    def cross_drive(path, start=None):
        raise ValueError("path is on mount 'C:', start on mount 'D:'")

    monkeypatch.setattr(mod.os.path, "relpath", cross_drive)
    found = mod.oversized_entries("", str(tmp_path / "changelog.d"))
    assert found and found[0][0] == str(frags / "long.md")


def test_the_gate_passes_under_a_cp1252_console(tmp_path: Path) -> None:
    """Run the gate with a cp1252 stdout, the encoding of a Windows pipe.

    The fragment carries a section sign and the report an em dash; both
    must print without raising. Kept so Linux catches an encoding regression.
    """
    r = _run(tmp_path, 1_000, slot="cp", n_fragment_chars=0)
    assert r.returncode == 0, r.stdout + r.stderr
    frag = tmp_path / "cp" / "changelog.d" / "added" / "sect.md"
    frag.write_text(
        "- **Sign.** see [§1](docs/design/async-dsss-receiver.md)\n",
        encoding="utf-8",
    )
    env = {**os.environ, "PYTHONIOENCODING": "cp1252"}
    r = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--changelog",
            str(tmp_path / "cp" / "CHANGELOG.md"),
            "--fragments",
            str(tmp_path / "cp" / "changelog.d"),
        ],
        capture_output=True,
        text=True,
        encoding="cp1252",
        env=env,
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK" in r.stdout
