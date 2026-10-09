#!/usr/bin/env python3
"""Gate: every published benchmark names a commit that main can reach.

Each `benchmarks/published/v<ver>/*.json` stamps `doppler_meta.commit`, and
`docs/benchmarks.md` prints it as the page's provenance ("measured ...,
doppler `<sha>`"). The promise is that a reader can check that commit out
and re-run the numbers. A commit that is not on main breaks the promise
quietly: `git show <sha>` fails in a fresh clone, and the numbers become
reproducible only by whoever measured them.

It is not hypothetical. A set measured on a release branch and then
squash- or rebase-merged names a commit that main never receives. v0.48.0
(#1319) and v0.65.0 were caught by eye and restamped by hand. v0.56.0,
v0.57.0, v0.58.0 and v0.64.0 were not caught, and they are why the
exemption list below exists (#1322).

**What it checks.** Every `doppler_meta.commit` under
`benchmarks/published/*/*.json`, found by globbing, is an ancestor of
`--base` (`origin/main` from the Makefile). A new release directory is
checked the moment it exists.

**History.** `git merge-base --is-ancestor` needs the objects. A shallow
clone fails here with a message that names the depth, not the snapshot,
because a shallow clone cannot tell "not on main" from "not fetched". CI's
lint job checks out with `fetch-depth: 0`.

**Existing breakage** is in `scripts/.bench-commit-exempt`, one release
directory per line with its reason. The list may only shrink: an entry
whose set now passes, or whose directory is gone, fails as stale.

Usage:  python3 scripts/check_bench_commits.py [--base REF] [--root DIR]
Exit 0 when every published set names a commit main can reach.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PUBLISHED = "benchmarks/published"
EXEMPT = "scripts/.bench-commit-exempt"


def _git(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(root), *args], capture_output=True, text=True
    )


def _stamps(root: Path) -> dict[tuple[str, str], list[str]]:
    """(release dir, stamped commit) -> the files that carry it."""
    found: dict[tuple[str, str], list[str]] = defaultdict(list)
    for path in sorted((root / PUBLISHED).glob("*/*.json")):
        meta = json.loads(path.read_text(encoding="utf-8")).get(
            "doppler_meta", {}
        )
        commit = meta.get("commit") or "(no doppler_meta.commit)"
        found[(path.parent.name, commit)].append(path.name)
    return found


def _exemptions(root: Path) -> tuple[dict[str, str], list[str]]:
    """Release dir -> reason, plus any malformed lines."""
    exempt: dict[str, str] = {}
    bad: list[str] = []
    path = root / EXEMPT
    if not path.exists():
        return exempt, bad
    for n, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        release, _, reason = line.partition(" ")
        if not reason.strip():
            bad.append(f"{EXEMPT}:{n}: {release} has no reason")
        exempt[release] = reason.strip()
    return exempt, bad


def _verdict(root: Path, commit: str, base: str) -> str | None:
    """None when ``commit`` is an ancestor of ``base``, else why not."""
    resolved = _git(
        root, "rev-parse", "--verify", "-q", f"{commit}^{{commit}}"
    )
    if resolved.returncode != 0:
        return "not in this repository's history"
    sha = resolved.stdout.strip()
    rc = _git(root, "merge-base", "--is-ancestor", sha, base).returncode
    if rc == 0:
        return None
    if rc == 1:
        return f"not an ancestor of {base}"
    return f"git merge-base failed (exit {rc})"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", default="origin/main")
    # --root exists for this gate's OWN test, which builds a throwaway repo
    # with a main branch and a snapshot that names an off-main commit.
    ap.add_argument("--root", type=Path, default=ROOT)
    args = ap.parse_args()
    root, base = args.root, args.base

    if _git(root, "rev-parse", "--is-shallow-repository").stdout.strip() == (
        "true"
    ):
        print(
            "bench-commits: this clone is SHALLOW, so whether a stamped "
            "commit is on\n"
            f"  {base} cannot be decided -- a missing object is indistinct "
            "from an\n"
            "  unfetched one. Fetch full history (`git fetch --unshallow`; "
            "in CI,\n"
            "  `fetch-depth: 0` on the job that runs `make lint`)."
        )
        return 1
    if _git(root, "rev-parse", "--verify", "-q", base).returncode != 0:
        print(
            f"bench-commits: base ref {base} not found -- fetch it, or set "
            "BENCH_COMMIT_BASE."
        )
        return 1

    exempt, problems = _exemptions(root)
    failing: dict[str, list[str]] = defaultdict(list)
    for (release, commit), files in sorted(_stamps(root).items()):
        why = _verdict(root, commit, base)
        if why is not None:
            failing[release].append(
                f"{PUBLISHED}/{release}: doppler_meta.commit {commit} is "
                f"{why} ({', '.join(files)})"
            )

    new = [
        line
        for r, lines in failing.items()
        if r not in exempt
        for line in lines
    ]
    stale = [
        f"{EXEMPT}: {r} is exempt but "
        + (
            "its directory is gone"
            if not (root / PUBLISHED / r).is_dir()
            else "now passes -- delete the line"
        )
        for r in sorted(exempt)
        if r not in failing
    ]
    problems += new + stale
    if not problems:
        print(
            f"bench-commits: every published set names a commit on {base} "
            f"({len(exempt)} exempt)"
        )
        return 0
    print("bench-commits: a published set names a commit main cannot reach --")
    for line in problems:
        print(f"  {line}")
    if new:
        print(
            "  A reader must be able to check the stamped commit out.\n"
            "  Measure from a commit already on main, or run\n"
            "  `make bench-restamp VERSION=X.Y.Z` to move the stamp onto\n"
            "  main's commit with the IDENTICAL tree (commit_info.id, the\n"
            "  literal checkout, stays). Never add a new exemption: the list\n"
            "  holds sets that can no longer be fixed, and it only shrinks."
        )
    return 1


if __name__ == "__main__":
    sys.exit(main())
