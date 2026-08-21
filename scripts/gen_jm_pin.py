#!/usr/bin/env python3
"""One source of truth for the just-makeit pin, and a gate that proves it.

doppler states the jm version in THREE places, and they must agree:

===========================================  ==================================
site                                         spelling
===========================================  ==================================
``just-makeit.toml`` ``[project] jm_version``  the SSOT -- what jm itself reads
``pyproject.toml`` dev group                   ``just-makeit==X.Y.Z``
``examples/downstream-jm/just-makeit.toml``    ``jm_version`` again
===========================================  ==================================

Nothing enforced that before this script. A bump is three hand edits, and a
missed one is **invisible**: verified by mutation on 2026-08-07 -- reverting
the downstream pin alone left ``make drift-check`` exiting **0**, with jm's
gh-183 skew notice reduced to one ``warning:`` line inside a wall of advisory
output. ``drift-check``'s own comment says the example "cannot silently
document a jm version doppler is not on", which was an intent, not a mechanism.

``scripts/check_version_strings.py`` does not cover this: it guards
*doppler's* release version against being hand-typed into docs -- a different
number entirely.

The pin must also be RECORDED
-----------------------------
``--check`` additionally asserts the pinned version appears in
``CHANGELOG.md`` on a line naming the jm pin. Agreement across three files
says the bump was applied consistently; it says nothing about whether anyone
was told.

This is checked here, in the script that already owns the pin, rather than in
a second gate that would have to parse ``jm_version`` for itself -- the
version is read once, from the SSOT, and everything asserted about it hangs
off that read.

``make changelog-check`` cannot cover it: that gate fails only when
``[Unreleased]`` has **zero** entries, so a bump lands green under any other
bullet. Demonstrated rather than assumed -- ``4f1eb86b`` moved all three pin
sites to 0.55.3, retired a hand-patched carve-out and fixed a large-n
truncation on the NCO path, and touched no CHANGELOG at all. The version it
shipped was absent from that file and jm's gh-920 was mentioned nowhere.

``--write`` deliberately does NOT fix this half. A derived pin string is
generated; the sentence explaining what a bump brought is not, and a gate that
auto-inserted a placeholder would trade a missing entry for a meaningless one.

Recorded WHEN, not merely somewhere (gh-693)
--------------------------------------------
Asking whether the version appears anywhere in the changelog is not enough,
and the hole is a **rollback**. "0.63.3 regressed us, go back to 0.55.3" moves
all three sites to a version the file already describes *arriving at*, so any
whole-file scan finds it and the bump ships announced by nobody.

So when the pin MOVED on this branch -- measured against the MERGE BASE, the
same question and the same baseline the assertion ratchet uses -- the move must
be announced by *this change*: the version has to appear as a pin destination
among the changelog lines the branch ADDS. History does not count, because
history is what a rollback returns to.

Two scopes were tried first and are wrong, both for the same reason:
``[Unreleased]`` is 198 KB here (doppler has not released since v0.42.0) and
holds every pin bump since 0.52.0; ``changelog.d/`` is 109 unassembled
fragments. Each is this release *cycle's* history, not this *branch's*
statement, and scoping to either let the rollback pass green.

gh-693 proposed instead taking the LAST semver on a pin line. This file's own
history refutes it twice: ``pin 0.57.0 -> 0.59.0, and the create-only headers
0.58.0 ships`` ends on a version that is neither side of the move, and
``pinned to 0.25.0 (from 0.24.0)`` puts the destination FIRST. The destination
is therefore read from what the sentence does, not from position -- see
``_line_destination``.

Usage
-----
::

    python scripts/gen_jm_pin.py --check   # exit 1 on disagreement or if
                                           # the pin is unrecorded
    python scripts/gen_jm_pin.py --write   # rewrite the derived sites
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

import tomllib

ROOT = Path(__file__).resolve().parent.parent

SSOT = ROOT / "just-makeit.toml"
PYPROJECT = ROOT / "pyproject.toml"
DOWNSTREAM = ROOT / "examples" / "downstream-jm" / "just-makeit.toml"
CHANGELOG = ROOT / "CHANGELOG.md"
FRAGMENTS = ROOT / "changelog.d"

# A line that is talking about the jm pin. Deliberately loose about the
# surrounding markdown (bold, backticks, "Carried by the ...") and strict about
# the subject, so prose mentioning some other 0.x.y cannot satisfy the check.
PIN_LINE_RE = re.compile(r"just-makeit\s+pin(?:ned)?", re.IGNORECASE)
SEMVER_RE = re.compile(r"\d+\.\d+\.\d+")

# The version a pin line says the pin moved TO. gh-693: harvesting every semver
# on the line counted a LEFT-HAND side as recorded, so 17 of 18 "recorded"
# versions were ones the changelog describes moving AWAY from.
#
# gh-693 proposed taking the LAST semver on the line, which this file's own
# history refutes twice. `pin 0.57.0 -> 0.59.0, and the create-only headers
# 0.58.0 ships` ends on a version that is neither side of the move, and
# `pinned to 0.25.0 (from 0.24.0)` puts the destination FIRST. So the
# destination is found by what the sentence DOES, in the three shapes this
# changelog actually uses, rather than by position:
#
#   `X -> Y`            the pair: the destination is Y
#   `-> Y` / `to Y`     a bare move with no source named
#   anything else       the first semver, with an explicit "(from X)" removed
ARROW_PAIR_RE = re.compile(
    r"(\d+\.\d+\.\d+)\s*(?:->|\u2192|-->)\s*\**`?(\d+\.\d+\.\d+)"
)
ARROW_TO_RE = re.compile(r"(?:->|\u2192|-->|\bto)\s*\**`?(\d+\.\d+\.\d+)")
FROM_RE = re.compile(r"\(?\bfrom\s+\**`?\d+\.\d+\.\d+\)?")

# Anchored to the start of a line so a version quoted in PROSE cannot match --
# just-makeit.toml carries commentary naming older jm versions, and a comment
# is history, not a claim about the present pin.
JM_VERSION_RE = re.compile(r'^(jm_version\s*=\s*")([^"]+)(")', re.MULTILINE)
# The dev-group requirement. `==` only: a range would make "the pinned version"
# unanswerable, which is the whole point of pinning it here.
DEP_RE = re.compile(r'^(\s*"just-makeit==)([^"]+)(",?\s*)$', re.MULTILINE)


def ssot_version() -> str:
    """Read the pin every other site must match."""
    with SSOT.open("rb") as fh:
        data = tomllib.load(fh)
    version = data.get("project", {}).get("jm_version")
    if not version:
        raise SystemExit(f"gen_jm_pin: no [project] jm_version in {SSOT.name}")
    return str(version)


def recorded_versions() -> set[str]:
    """Every jm version the changelog names on a jm-pin line.

    Whole file, not just ``[Unreleased]``: a release moves entries into a
    versioned section, and a bump recorded then is still recorded.

    ``changelog.d/`` is read too, and it has to be. An entry now lands as a
    FRAGMENT and is only folded into ``CHANGELOG.md`` at release
    (``changelog.d/README.md``), so a bump PR records its pin in a file this
    check would otherwise never open — and would then report a pin nothing
    documents, on a branch that documents it perfectly well.
    """
    return _pin_destinations(_changelog_texts())


def _changelog_texts() -> list[str]:
    """The changelog plus every unassembled fragment."""
    texts = [CHANGELOG.read_text(encoding="utf-8")]
    if FRAGMENTS.is_dir():
        texts += [
            f.read_text(encoding="utf-8")
            for f in sorted(FRAGMENTS.glob("*/*.md"))
        ]
    return texts


def _line_destination(line: str) -> str | None:
    """The version this pin line says the pin moved TO, or None."""
    m = ARROW_PAIR_RE.search(line)
    if m:
        return m.group(2)
    m = ARROW_TO_RE.search(line)
    if m:
        return m.group(1)
    found = SEMVER_RE.findall(FROM_RE.sub("", line))
    return found[0] if found else None


def _pin_destinations(texts: list[str]) -> set[str]:
    out: set[str] = set()
    for text in texts:
        for line in text.splitlines():
            if PIN_LINE_RE.search(line):
                dest = _line_destination(line)
                if dest:
                    out.add(dest)
    return out


def branch_added_text(base: str) -> str | None:
    """The changelog lines THIS BRANCH adds, or None if git cannot say.

    Not the `[Unreleased]` section, which was the obvious scope and is wrong
    here: doppler has not released since v0.42.0, so `[Unreleased]` is 198 KB
    of accumulated history holding every pin bump since 0.52.0 -- including
    the ones a rollback would return to. Scoping to it let the rollback pass
    green, which is the bug being closed rather than a smaller version of it.

    `changelog.d/` fails the same way for the same reason: 109 unassembled
    fragments are this release CYCLE's history, not this BRANCH's statement.

    The added lines are the only scope that means "announced by this change",
    and a new fragment file is entirely added lines, so the ordinary way of
    recording a bump satisfies it naturally.
    """
    ref = merge_base(base)
    if ref is None:
        return None
    diff = subprocess.run(
        [
            "git",
            "diff",
            "--unified=0",
            ref,
            "--",
            "CHANGELOG.md",
            "changelog.d/",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if diff.returncode != 0:
        return None
    added = [
        line[1:]
        for line in diff.stdout.splitlines()
        if line.startswith("+") and not line.startswith("+++")
    ]
    # An UNTRACKED fragment is entirely new and `git diff` cannot see it. The
    # ordinary way to record a bump is to write a fresh changelog.d/ file, so
    # without this the gate rejects exactly the workflow it is asking for --
    # a false positive, which is how a gate gets switched off.
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "changelog.d/"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if untracked.returncode == 0:
        for rel in untracked.stdout.split():
            f = ROOT / rel
            if f.is_file():
                added += f.read_text(encoding="utf-8").splitlines()
    return "\n".join(added)


def merge_base(base: str) -> str | None:
    """The merge base with @p base, or None if git cannot resolve it."""
    mb = subprocess.run(
        ["git", "merge-base", "HEAD", base],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if mb.returncode == 0 and mb.stdout.strip():
        return mb.stdout.strip()
    rev = subprocess.run(
        ["git", "rev-parse", "--verify", f"{base}^{{commit}}"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return base if rev.returncode == 0 else None


def base_pin(base: str) -> str | None:
    """The pin at the merge base with @p base, or None if git cannot say.

    The merge base rather than the tip, for the reason the assertion ratchet
    gives: the question is "did THIS branch move the pin", and a branch that
    is merely BEHIND has not moved anything. Compared against the tip of
    `origin/main`, every branch cut before someone else's bump would be asked
    to announce a bump it did not make.
    """
    ref = merge_base(base)
    if ref is None:
        return None
    show = subprocess.run(
        ["git", "show", f"{ref}:{SSOT.name}"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if show.returncode != 0:
        return None
    m = JM_VERSION_RE.search(show.stdout)
    return m.group(2) if m else None


def sites(want: str) -> list[tuple[Path, re.Pattern[str], str]]:
    """Every derived site: (path, pattern, the text it should hold)."""
    return [
        (PYPROJECT, DEP_RE, want),
        (DOWNSTREAM, JM_VERSION_RE, want),
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    ap.add_argument(
        "--base",
        default="origin/main",
        help="ref whose MERGE BASE decides whether this branch "
        "moved the pin (default: origin/main)",
    )
    args = ap.parse_args()

    want = ssot_version()
    bad: list[str] = []
    wrote: list[str] = []

    for path, pattern, expected in sites(want):
        text = path.read_text(encoding="utf-8")
        found = pattern.search(text)
        if not found:
            bad.append(f"{path.relative_to(ROOT)}: no just-makeit pin found")
            continue
        got = found.group(2)
        if got == expected:
            continue
        if args.write:
            path.write_text(
                pattern.sub(rf"\g<1>{expected}\g<3>", text, count=1),
                encoding="utf-8",
            )
            wrote.append(f"{path.relative_to(ROOT)}: {got} -> {expected}")
        else:
            bad.append(
                f"{path.relative_to(ROOT)}: has {got}, "
                f"{SSOT.name} says {expected}"
            )

    if bad:
        print("gen_jm_pin: the just-makeit pin disagrees across sites:")
        for line in bad:
            print(f"  {line}")
        print(f"\n  fix: python {Path(__file__).name} --write")
        return 1

    if args.write:
        for line in wrote:
            print(f"gen_jm_pin: {line}")
        print(f"gen_jm_pin: all sites pinned to {want}")
        return 0

    # Applied consistently is not the same as announced. --write cannot fix
    # this one, so it is asserted only in --check.
    #
    # When the pin MOVED on this branch, history does not count: the move has
    # to be announced by THIS change. That is what closes gh-693's rollback --
    # "0.55.4 regressed us, go back to 0.55.3" moves all three sites to a
    # version that was legitimately a destination once, so any whole-file scan
    # finds it and says nothing. Asking whether the pin moved is the only
    # question that separates the two cases, and it does not depend on parsing
    # prose at all.
    was = base_pin(args.base)
    if was is not None and was != want:
        added = branch_added_text(args.base)
        if added is not None and want not in _pin_destinations([added]):
            print(
                f"gen_jm_pin: this branch moves the pin {was} -> {want}, "
                f"and nothing announces it."
            )
            print(
                f"  Add a changelog.d/ fragment naming the move, e.g.\n"
                f'    "**just-makeit pin {was} → {want}.**" '
                f"and what it brought."
            )
            print(
                "  A version named anywhere in CHANGELOG.md history does NOT "
                "count here:\n  a ROLLBACK returns to a version the file "
                "already describes arriving at,\n  which is how this passed "
                "green while saying nothing (gh-693)."
            )
            return 1
        return 0

    if want not in recorded_versions():
        print(
            f"gen_jm_pin: the pin is {want} in all 3 sites, but "
            f"{CHANGELOG.name} never records it."
        )
        print(
            f"  Add an entry naming the bump, e.g. "
            f'"**just-makeit pin <previous> → {want}.**", under '
            f"[Unreleased]."
        )
        print(
            "  `make changelog-check` cannot catch this: it only fails on an "
            "EMPTY\n  [Unreleased], so a bump ships green under any other "
            "bullet."
        )
        return 1

    print(f"gen_jm_pin: just-makeit pinned to {want} in all 3 sites, recorded")
    return 0


if __name__ == "__main__":
    sys.exit(main())
