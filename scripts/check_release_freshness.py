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


def stale_benchmarks(
    changed: list[str], version: str, cwd: pathlib.Path
) -> list[str]:
    """Perf-relevant paths that moved with no snapshot for *version*.

    Returns the moved paths when the release has no published numbers, else
    an empty list. The snapshot is measured by hand on a representative
    machine (release.md §2b) precisely because CI runners are not, so this
    can only check that it EXISTS -- not that it is right.
    """
    moved = [c for c in changed if c.startswith(PERF_PATHS)]
    if not moved:
        return []
    if (cwd / SNAPSHOTS / f"v{version}").is_dir():
        return []
    return moved


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
    gal = stale_gallery(changed, args.scripts)
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

    if rc == 0:
        print(
            f"release-freshness: OK — gallery and {SNAPSHOTS}/v{args.version} "
            f"are current as of {ref}"
        )
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
