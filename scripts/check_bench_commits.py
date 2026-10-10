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
v0.57.0, v0.58.0 and v0.64.0 were not caught (#1322). The last two were
restamped onto their tree-identical commits on main with
`make bench-restamp`. The first two cannot be, and they are the exemption
list below.

**What it checks.** Every `doppler_meta.commit` under
`benchmarks/published/*/*.json`, found by globbing, is an ancestor of
`--base` (`origin/main` from the Makefile). A new release directory is
checked the moment it exists.

**History.** `git merge-base --is-ancestor` needs the objects. A shallow
clone fails here with a message that names the depth, not the snapshot,
because a shallow clone cannot tell "not on main" from "not fetched". CI's
lint job checks out with `fetch-depth: 0`.

**Existing breakage** is in `scripts/.bench-commit-exempt`, one line per
exempt (set, commit) PAIR with its reason: `v0.56.0 c9dbcf8da <why>`. The
pair, not the directory, so a new file dropped into an exempt set with
some other off-main stamp still fails, and so does re-stamping an exempt
file. The list may only shrink. An entry ADDED since the merge base with
`--base` fails (`_gitbase.added_since_base`, where a list absent at the
base held nothing). So does an entry whose pair no longer fails or whose
directory is gone.

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

from _gitbase import BaseUnreadableError, added_since_base, in_git_repo

ROOT = Path(__file__).resolve().parent.parent
PUBLISHED = "benchmarks/published"
EXEMPT = "scripts/.bench-commit-exempt"
SELF = "scripts/check_bench_commits.py"  # the ratchet starts when this lands


def git(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    """Run git in ``root``; shared with ``bench_restamp.py``."""
    return subprocess.run(
        ["git", "-C", str(root), *args], capture_output=True, text=True
    )


def resolve(root: Path, ref: str) -> str | None:
    """The full SHA ``ref`` names as a commit, or None."""
    r = git(root, "rev-parse", "--verify", "-q", f"{ref}^{{commit}}")
    return r.stdout.strip() if r.returncode == 0 else None


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


def _exemptions(text: str) -> tuple[dict[tuple[str, str], str], list[str]]:
    """(release dir, stamped commit) -> reason, plus any malformed lines."""
    exempt: dict[tuple[str, str], str] = {}
    bad: list[str] = []
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        release, commit, reason = ([*line.split(None, 2), "", ""])[:3]
        if not reason:
            bad.append(
                f"{EXEMPT}:{n}: needs `<release> <commit> <reason>`: {line}"
            )
        exempt[(release, commit)] = reason
    return exempt, bad


def verdict(root: Path, commit: str, base: str) -> str | None:
    """None when ``commit`` is an ancestor of ``base``, else why not.

    Shared with ``bench_restamp.py``, which leaves a stamp alone when this
    says None.
    """
    if resolve(root, base) is None:
        return f"not checkable: {base} is not here -- fetch it"
    sha = resolve(root, commit)
    if sha is None:
        return "not in this repository's history"
    rc = git(root, "merge-base", "--is-ancestor", sha, base).returncode
    if rc == 0:
        return None
    if rc != 1:
        return f"not checkable: git merge-base exited {rc}"
    # In this checkout but not on base. Which advice comes first depends on
    # where else the commit is: on no remote branch, it was never pushed,
    # and no fetch will find it on main.
    remote = git(root, "branch", "-r", "--contains", sha).stdout.split()
    if not remote:
        return (
            f"not on {base}, and on no remote branch: it was never pushed. "
            f"Land it, or measure a commit {base} has"
        )
    return (
        f"not on {base}, though {', '.join(remote[:2])} has it -- if it "
        f"has merged since, fetch {base} first"
    )


def _added(
    root: Path, base: str, exempt: dict[tuple[str, str], str]
) -> list[str]:
    """Exempt pairs the merge base with ``base`` did not have."""
    added = added_since_base(
        root,
        base,
        EXEMPT,
        sorted(exempt),
        lambda text: _exemptions(text)[0],
        since=SELF,
    )
    return [
        f"{EXEMPT}: {r} {c} was ADDED -- the list may only shrink"
        for r, c in added
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", default="origin/main")
    # --root exists for this gate's OWN test, which builds a throwaway repo
    # with a main branch and a snapshot that names an off-main commit.
    ap.add_argument("--root", type=Path, default=ROOT)
    args = ap.parse_args()
    root, base = args.root, args.base

    if not in_git_repo(root):
        print(
            f"bench-commits: {root} is not a git checkout, so no stamped "
            "commit can be\n  checked against main. Run it in a clone."
        )
        return 1
    if git(root, "rev-parse", "--is-shallow-repository").stdout.strip() == (
        "true"
    ):
        depth = git(root, "rev-list", "--count", "HEAD").stdout.strip()
        print(
            f"bench-commits: this clone is SHALLOW ({depth} commit(s) of "
            "history), so\n"
            f"  whether a stamped commit is on {base} cannot be decided -- "
            "a missing\n"
            "  object is indistinct from an unfetched one. Fetch full "
            "history\n"
            "  (`git fetch --unshallow`; in CI, `fetch-depth: 0` on the job "
            "that runs\n"
            "  `make lint`)."
        )
        return 1
    if resolve(root, base) is None:
        print(
            f"bench-commits: base ref {base} not found -- fetch it, or set "
            "BENCH_COMMIT_BASE."
        )
        return 1

    path = root / EXEMPT
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    exempt, problems = _exemptions(text)
    try:
        problems += _added(root, base, exempt)
    except BaseUnreadableError:
        print(
            f"bench-commits: cannot read {EXEMPT} at the merge base with "
            f"{base},\n  so an ADDED exemption cannot be told from an old "
            "one. Fetch the base."
        )
        return 1
    failing: dict[tuple[str, str], str] = {}
    for (release, commit), files in sorted(_stamps(root).items()):
        why = verdict(root, commit, base)
        if why is not None:
            failing[(release, commit)] = (
                f"{PUBLISHED}/{release}: doppler_meta.commit {commit} is "
                f"{why} ({', '.join(files)})"
            )

    new = [line for pair, line in failing.items() if pair not in exempt]
    stale = [
        f"{EXEMPT}: {r} {c} is exempt but "
        + (
            "its directory is gone"
            if not (root / PUBLISHED / r).is_dir()
            else "no file there fails with that stamp -- delete the line"
        )
        for r, c in sorted(exempt)
        if (r, c) not in failing
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
        unpushed = any("never pushed" in line for line in new)
        fetch = (
            '  1. A commit marked "never pushed" is only in this checkout:\n'
            "     land it first, or measure a commit main has. For the rest,\n"
            if unpushed
            else "  1. "
        )
        print(
            "  A reader must be able to check the stamped commit out. In\n"
            "  this order:\n"
            f"{fetch}`git fetch origin` -- a stale {base} reports a merged\n"
            "     commit as missing.\n"
            "  2. `make bench-restamp VERSION=X.Y.Z` -- moves the stamp onto\n"
            "     main's commit with the IDENTICAL tree. Only possible when\n"
            "     the measured tree landed unchanged (a rebase-merge of it,\n"
            "     never a squash); commit_info.id, the checkout, stays.\n"
            "  3. Otherwise re-measure from a commit on main\n"
            "     (docs/dev/release.md section 2b).\n"
            "  Never add an exemption: the list holds sets that can no\n"
            "  longer be fixed, and it only shrinks."
        )
    return 1


if __name__ == "__main__":
    sys.exit(main())
