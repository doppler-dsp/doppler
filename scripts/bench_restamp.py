#!/usr/bin/env python3
"""Restamp a published benchmark set onto main's tree-identical commit.

A set is measured at some commit, and `doppler_meta.commit` records it.
When that commit lives on a release branch that is then squash- or
rebase-merged, main receives the same CONTENT under a different SHA, and
the stamp names a commit a reader cannot check out.
`make bench-commits-check` refuses that (#1322). This is the fix: the
numbers were measured on a tree, and the commit on main with exactly that
tree is the honest provenance for them.

For each `benchmarks/published/v<VERSION>/*.json`:

1. Resolve `doppler_meta.commit`. If the object is not local, fetch
   `commit_info.id`: pytest-benchmark's record of the literal checkout,
   the full SHA the abbreviation was cut from.
2. Take its tree and list the commits on `--base` with the same tree.
   Exactly one is required. Zero means the measured content never landed
   as-is, and two means the answer is ambiguous; both are refused, naming
   what was found.
3. Rewrite ONLY the `doppler_meta.commit` value, at the same abbreviation
   length. The file is edited as text, and the edit must match exactly
   once; the result is parsed back and must differ from the original in
   that one field. `commit_info.id` stays as it was: it records what was
   actually checked out, and the two disagreeing is the trace of a
   restamp, as in #1321.

A stamp already on `--base` is left alone, so a second run changes
nothing. Nothing is written unless every file in the set can be restamped.

Usage:  python3 scripts/bench_restamp.py VERSION [--base REF] [--root DIR]
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PUBLISHED = "benchmarks/published"


class RestampError(Exception):
    """A file in the set cannot be restamped; nothing is written."""


def _git(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(root), *args], capture_output=True, text=True
    )


def _resolve(root: Path, ref: str) -> str | None:
    r = _git(root, "rev-parse", "--verify", "-q", f"{ref}^{{commit}}")
    return r.stdout.strip() if r.returncode == 0 else None


def _target(root: Path, name: str, data: dict, base: str) -> str | None:
    """The abbreviated commit to stamp, or None when already on base."""
    old = data["doppler_meta"]["commit"]
    sha = _resolve(root, old)
    full = data.get("commit_info", {}).get("id", "")
    if sha is None and full.startswith(old):
        _git(root, "fetch", "-q", "origin", full)
        sha = _resolve(root, old)
    if sha is None:
        raise RestampError(
            f"{name}: {old} is not in this repository and could not be "
            f"fetched (commit_info.id {full or 'absent'})"
        )
    if _git(root, "merge-base", "--is-ancestor", sha, base).returncode == 0:
        return None
    tree = _git(root, "rev-parse", f"{sha}^{{tree}}").stdout.strip()
    log = _git(root, "log", "--format=%H %T", base).stdout.split("\n")
    matches = [h for h, _, t in (ln.partition(" ") for ln in log) if t == tree]
    if len(matches) != 1:
        found = ", ".join(m[:12] for m in matches) or "none"
        raise RestampError(
            f"{name}: {old} has tree {tree[:12]}, and {len(matches)} "
            f"commit(s) on {base} have it ({found}); restamping needs "
            f"exactly one"
        )
    new = matches[0][: len(old)]
    if _resolve(root, new) != matches[0]:
        raise RestampError(
            f"{name}: {new} is ambiguous at {len(old)} characters"
        )
    return new


def _rewrite(name: str, text: str, old: str, new: str) -> str:
    """``text`` with doppler_meta.commit changed, and nothing else."""
    pat = re.compile(r'("commit"\s*:\s*")' + re.escape(old) + '"')
    hits = pat.findall(text)
    if len(hits) != 1:
        raise RestampError(
            f'{name}: "commit": "{old}" appears {len(hits)} times; the '
            f"rewrite needs exactly one"
        )
    out = pat.sub(lambda m: m.group(1) + new + '"', text)
    want = json.loads(text)
    want["doppler_meta"]["commit"] = new
    if json.loads(out) != want:
        raise RestampError(
            f"{name}: the edit changed more than doppler_meta.commit"
        )
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("version", help="X.Y.Z, as the release directory names it")
    ap.add_argument("--base", default="origin/main")
    # --root exists for this script's own test, which builds a throwaway
    # repository whose main has 0, 1 or 2 commits with the measured tree.
    ap.add_argument("--root", type=Path, default=ROOT)
    args = ap.parse_args()
    root = args.root
    d = root / PUBLISHED / f"v{args.version.removeprefix('v')}"
    files = sorted(d.glob("*.json"))
    if not files:
        print(f"bench-restamp: no published set at {d}")
        return 1

    edits: dict[Path, tuple[str, str, str]] = {}
    try:
        for path in files:
            text = path.read_text(encoding="utf-8")
            data = json.loads(text)
            new = _target(root, path.name, data, args.base)
            if new is not None:
                old = data["doppler_meta"]["commit"]
                edits[path] = (old, new, _rewrite(path.name, text, old, new))
    except RestampError as e:
        print(f"bench-restamp: refused, nothing written -- {e}")
        return 1

    for path, (old, new, out) in edits.items():
        path.write_text(out, encoding="utf-8")
        print(f"bench-restamp: {d.name}/{path.name}: {old} -> {new}")
    if not edits:
        print(f"bench-restamp: {d.name} already names commits on {args.base}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
