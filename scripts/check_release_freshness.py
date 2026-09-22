#!/usr/bin/env python3
"""Refuse a tag whose gallery plots or published benchmarks are stale.

`docs/dev/release.md` §2 and §2b both say "regenerate if it changed since the
last release", and nothing checked either. Guidance without a gate is the
thing this repo keeps re-learning: the comparison links were asked for by
hand until three releases shipped without them (#996), and `changelog.d/`
fragments sat unassembled until `changelog-assembled-check` was given an
execution home.

Both checks are DIFF-based, never regenerate-and-compare. A gallery PNG is
not guaranteed byte-stable across a regeneration, so a gate that re-rendered
and diffed would flap -- and a flapping gate is worse than no gate, because
it gets disabled and takes the real signal with it.

Home: a `tag-release` prerequisite, like `changelog-assembled-check`, and for
the same reason. `lint` is the wrong place -- a feature branch legitimately
changes a gallery script without re-rendering its plot, so it would be red on
every PR and get deleted. The one moment the question means anything is the
irreversible one.

Usage
-----
``make release-freshness-check VERSION=x.y.z`` -- the only supported entry
point. The gallery scripts are passed IN from the Makefile's
``GALLERY_SCRIPTS`` rather than restated here: a second list is one that can
disagree with the first.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent

#: Paths whose movement makes a published benchmark snapshot stale. Derived
#: from how a regression is actually attributed: #1342 pinned a 3x on
#: wfm_writer by observing that "since v0.48.0 only native/src/wfm_writer and
#: native/src/cvt moved". The C kernels are what the published numbers
#: measure, so they are what invalidates them.
PERF_PATHS = ("native/src", "native/inc")

#: Where a release's measured numbers live, one directory per version.
SNAPSHOTS = "benchmarks/published"

#: Regenerated plots land here.
ASSETS = "docs/assets"

#: A release may WAIVE an item here, one file per version. OUTSIDE docs/:
#: every page under docs/ must be linked from the index nearest it
#: (check_nav_index), and a per-release record would add nav churn for
#: something no reader browses to. The gate is
#: path-granular by design (see `code_changed` for the one narrowing it
#: already makes), so it cannot see that a change is confined to a platform
#: this release does not measure -- v0.55.0's `dp_complex.h` edit lives
#: entirely inside `#ifdef _WIN32`, and its gallery script gained a
#: Windows-only early return that re-renders byte-identically.
#:
#: A waiver is NOT a bypass: it names one item, carries a reason, is
#: committed beside the release, and is PRINTED by the gate, so a skipped
#: check appears in the release log rather than vanishing. An unused waiver
#: fails too -- a file that outlives its reason would silently widen.
WAIVERS = "release-waivers"

#: `- gallery: <path> -- <reason>` / `- benchmarks: <path> -- <reason>`
WAIVER_RE = re.compile(
    r"^-\s*(gallery|benchmarks)\s*:\s*(\S+)\s*(?:--|\u2014)\s*(\S.*)$"
)


def _git(*args: str, cwd: pathlib.Path) -> str:
    return subprocess.run(
        ["git", *args], cwd=cwd, capture_output=True, text=True, check=False
    ).stdout.strip()


def last_tag(cwd: pathlib.Path) -> str:
    """The most recent tag, or "" when the repo has none yet."""
    return _git("describe", "--tags", "--abbrev=0", cwd=cwd)


def changed_since(ref: str, cwd: pathlib.Path) -> list[str]:
    """Paths that moved between *ref* and HEAD."""
    if not ref:
        return []
    out = _git("diff", "--name-only", f"{ref}..HEAD", cwd=cwd)
    return [line for line in out.splitlines() if line]


def stale_gallery(changed: list[str], scripts: list[str]) -> list[str]:
    """Gallery scripts that moved while ``docs/assets/`` did not.

    The plot committed beside a script IS the documentation -- a gallery page
    ``--8<--`` includes regions of the script and shows the PNG next to it --
    so a script that changed without its plot being re-rendered publishes a
    picture of code that no longer exists.
    """
    touched = [s for s in scripts if s in changed]
    if not touched:
        return []
    if any(c.startswith(ASSETS + "/") for c in changed):
        return []
    return touched


def _is_comment(path: str, line: str) -> bool:
    """Is *line* (stripped) a comment, in *path*'s language?

    Conservative on purpose: a line this cannot classify counts as CODE, so a
    miss can only ask for benchmarks that were not needed -- never skip ones
    that were. In C that means only the unambiguous shapes: `*p = 1;` is code,
    `* the delay line` is a comment only because of the space.
    """
    if path.endswith(("CMakeLists.txt", ".cmake")):
        return line.startswith("#")
    if path.endswith((".c", ".h")):
        return line.startswith(("//", "/*", "*/", "* ")) or line == "*"
    return False


def code_changed(path: str, ref: str, cwd: pathlib.Path) -> bool:
    """Did *path* change anything but comments and blank lines since *ref*?

    v0.51.1's tag was refused for two CMakeLists.txt files whose only edit
    was a rewritten comment -- a 66-minute benchmark run demanded for prose.
    A path is still the first filter (a CMake flag can move performance);
    this only lets a diff that provably cannot through.
    """
    out = _git("diff", "-U0", f"{ref}..HEAD", "--", path, cwd=cwd)
    for raw in out.splitlines():
        if raw[:1] not in ("+", "-") or raw.startswith(("+++", "---")):
            continue
        line = raw[1:].strip()
        if line and not _is_comment(path, line):
            return True
    return False


def stale_benchmarks(
    changed: list[str], version: str, cwd: pathlib.Path
) -> list[str]:
    """Perf-relevant paths that moved with no snapshot for *version*.

    Returns the moved paths when the release has no published numbers, else
    an empty list. The snapshot is measured by hand on a representative
    machine (release.md §2b) precisely because CI runners are not, so this
    can only check that it EXISTS -- not that it is right.
    """
    ref = last_tag(cwd)
    moved = [
        c
        for c in changed
        if c.startswith(PERF_PATHS) and code_changed(c, ref, cwd)
    ]
    if not moved:
        return []
    if (cwd / SNAPSHOTS / f"v{version}").is_dir():
        return []
    return moved


def waivers(version: str, cwd: pathlib.Path) -> dict[tuple[str, str], str]:
    """``{(kind, path): reason}`` declared for *version*, or ``{}``.

    A line without a reason is not a waiver and is ignored, so the file
    cannot waive anything by accident: the reason is the point.
    """
    path = cwd / WAIVERS / f"v{version}.md"
    if not path.is_file():
        return {}
    out: dict[tuple[str, str], str] = {}
    key: tuple[str, str] | None = None
    for raw in path.read_text(encoding="utf-8").splitlines():
        m = WAIVER_RE.match(raw.strip())
        if m:
            key = (m.group(1), m.group(2))
            out[key] = m.group(3).strip()
            continue
        # A wrapped reason: mdformat reflows this file like any other
        # markdown, so a reason longer than the line width arrives as
        # indented continuations. Without joining them the gate would print
        # the first few words and call that the record.
        if key and raw[:1].isspace() and raw.strip():
            out[key] = f"{out[key]} {raw.strip()}"
        elif not raw.strip():
            key = None
    return out


def apply_waivers(
    kind: str, items: list[str], declared: dict[tuple[str, str], str]
) -> tuple[list[str], list[tuple[str, str]]]:
    """Split *items* into (still stale, waived) for one *kind*."""
    stale, waived = [], []
    for item in items:
        reason = declared.get((kind, item))
        if reason is None:
            stale.append(item)
        else:
            waived.append((item, reason))
    return stale, waived


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--version", required=True, metavar="X.Y.Z")
    ap.add_argument(
        "--repo",
        default=str(ROOT),
        help="repo root (a test points this at its fixture)",
    )
    ap.add_argument(
        "scripts", nargs="*", help="the Makefile's GALLERY_SCRIPTS"
    )
    args = ap.parse_args(argv)

    cwd = pathlib.Path(args.repo)
    ref = last_tag(cwd)
    if not ref:
        print("release-freshness: no tag yet — nothing to compare against")
        return 0
    changed = changed_since(ref, cwd)

    rc = 0
    declared = waivers(args.version, cwd)
    used: set[tuple[str, str]] = set()

    gal = stale_gallery(changed, args.scripts)
    gal, waived_gal = apply_waivers("gallery", gal, declared)
    for item, reason in waived_gal:
        used.add(("gallery", item))
        print(f"release-freshness: WAIVED gallery {item} — {reason}")
    if gal:
        rc = 1
        print(
            f"release-freshness: {len(gal)} gallery script(s) changed since "
            f"{ref}, and nothing under {ASSETS}/ did:"
        )
        for s in gal:
            print(f"    {s}")
        print(
            "  The committed PNG is what the gallery page shows beside the\n"
            "  script it includes, so this would publish a picture of code\n"
            "  that no longer exists. Run `make gallery`, commit docs/assets."
        )

    bench = stale_benchmarks(changed, args.version, cwd)
    bench, waived_bench = apply_waivers("benchmarks", bench, declared)
    for item, reason in waived_bench:
        used.add(("benchmarks", item))
        print(f"release-freshness: WAIVED benchmarks {item} — {reason}")
    if bench:
        rc = 1
        print(
            f"release-freshness: perf-relevant code changed since {ref} and "
            f"{SNAPSHOTS}/v{args.version}/ does not exist "
            f"({len(bench)} path(s), e.g. {bench[0]})."
        )
        print(
            "  release.md §2b: measure both builds interleaved on the\n"
            "  representative machine, then `make bench-publish`. Skipping\n"
            "  is only correct when no perf-relevant code moved — it did."
        )

    unused = sorted(set(declared) - used)
    if unused:
        rc = 1
        print(
            f"release-freshness: {len(unused)} waiver(s) in "
            f"{WAIVERS}/v{args.version}.md match nothing stale:"
        )
        for kind, item in unused:
            print(f"    {kind}: {item}")
        print(
            "  A waiver outliving its reason is how one silently widens\n"
            "  into a blanket skip. Delete the line, or name what is stale."
        )

    if rc == 0:
        print(
            f"release-freshness: OK — gallery and {SNAPSHOTS}/v{args.version} "
            f"are current as of {ref}"
        )
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
