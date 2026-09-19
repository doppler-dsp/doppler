#!/usr/bin/env python3
"""Gate: a passing test suite leaves no file behind.

Run a test command, and afterwards fail if any file it WROTE is still there --
in the source tree, or (``--ctest``) under any test's working directory.
"Wrote" means new since the run started, or modified during it: a test that
re-creates the same leftover on every run leaves nothing *new* after the
first, and a check on novelty alone passes a tree that leaks on every run.
That was exactly this tree's state: ``test_wfm_reader_core`` left 43 captures
in its ctest working directory, ``wfmgen_cli_test.cmake`` 23 and
``test_wfm_writer_core`` one, while every run reported green.

Run by hand from the repo root, the same tests dropped their files into the
root instead, where they sat gitignored and invisible to ``git status`` --
and pytest-benchmark created ``.benchmarks/`` there on every pytest run.

Scope and rules:

- A FAILING run is not checked: the command's own exit code is returned
    unchanged, and a failed test may keep its files as evidence.
- The source tree is read through ``git status --ignored``, so tracked files
    a test rewrote count too. A NEW directory is walked, and only its files
    that are not harness caches are reported.
- ``--ctest`` asks ctest for every test's ``WORKING_DIRECTORY``
    (``--show-only=json-v1``, same arguments) and walks each. Derived, not
    listed: a new test is covered the moment ctest knows it.
- Harness output is not a leak: ``__pycache__``, ``.pytest_cache``, ctest's
    own ``Testing/``, and coverage data (``.coverage*``, ``coverage.xml``).
    pytest-benchmark's ``.benchmarks/`` is deliberately NOT on that list: its
    storage is configured under ``build/``, and allowing it hid a pytest call
    that ran outside that configuration and recreated it in the root.

A test that writes files should write under a scratch directory it removes
(``test_wfm_reader_core.c`` is the C example), or delete what it wrote.

Usage:  python3 scripts/check_test_leaks.py [--root DIR] [--ctest] -- CMD...
Exit: CMD's code if it failed; else 1 if a file was left behind, else 0.
"""

from __future__ import annotations

import argparse
import contextlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Directory names whose contents are a tool's cache, wherever they appear.
CACHE_DIRS = {"__pycache__", ".pytest_cache", "Testing"}


def _is_cache(rel: str) -> bool:
    parts = rel.replace(os.sep, "/").split("/")
    if any(p in CACHE_DIRS for p in parts):
        return True
    name = parts[-1]
    return name == "coverage.xml" or name.startswith(".coverage")


def _git_entries(root: Path) -> dict[str, str]:
    """``{path: status}`` for every untracked, ignored or modified path."""
    out = subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "status",
            "--porcelain=v1",
            "-z",
            "--ignored=matching",
            "--untracked-files=all",
        ],
        check=True,
        capture_output=True,
    ).stdout.decode("utf-8", "surrogateescape")
    entries: dict[str, str] = {}
    recs = out.split("\0")
    i = 0
    while i < len(recs):
        rec = recs[i]
        i += 1
        if len(rec) < 4:
            continue
        status, path = rec[:2], rec[3:]
        if status[0] in "RC":  # a rename carries its source as the next record
            i += 1
        entries[path.rstrip("/")] = status
    return entries


def _walk(top: Path) -> dict[str, int]:
    """``{path: mtime_ns}`` for every file and directory under *top*."""
    seen: dict[str, int] = {}
    for dirpath, dirnames, filenames in os.walk(top):
        dirnames[:] = [d for d in dirnames if d not in CACHE_DIRS]
        for name in dirnames + filenames:
            p = os.path.join(dirpath, name)
            # Removed between listing and stat: then it was not left behind.
            with contextlib.suppress(OSError):
                seen[p] = os.lstat(p).st_mtime_ns
    return seen


def _ctest_dirs(cmd: list[str]) -> dict[str, list[str]]:
    """``{working_directory: [test names]}`` from ctest's own test list."""
    out = subprocess.run(
        [*cmd, "--show-only=json-v1"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    dirs: dict[str, list[str]] = {}
    for t in json.loads(out).get("tests", []):
        for p in t.get("properties", []):
            if p.get("name") == "WORKING_DIRECTORY":
                dirs.setdefault(os.path.realpath(p["value"]), []).append(
                    t["name"]
                )
    return dirs


def _roots(dirs: dict[str, list[str]]) -> list[str]:
    """The outermost directories: walking a parent covers its children."""
    out: list[str] = []
    for d in sorted(dirs):
        if not any(d.startswith(r.rstrip(os.sep) + os.sep) for r in out):
            out.append(d)
    return out


def _owner(path: str, dirs: dict[str, list[str]]) -> str:
    best = max(
        (d for d in dirs if path.startswith(d + os.sep)), key=len, default=None
    )
    if best is None:
        return ""
    names = dirs[best]
    more = f" +{len(names) - 3}" if len(names) > 3 else ""
    return f"  (cwd of {', '.join(names[:3])}{more})"


def leaks(
    root: Path,
    before_git: dict[str, str],
    after_git: dict[str, str],
    before_walk: dict[str, int],
    after_walk: dict[str, int],
    start_ns: int,
    skip: list[str],
) -> list[str]:
    """Paths a run left behind, relative to *root* where they are under it."""
    found: set[str] = set()

    def written(p: str, mtime: int | None) -> bool:
        return p not in before_walk or (mtime or 0) >= start_ns

    for d_path, mtime in after_walk.items():
        if os.path.isdir(d_path) and d_path in before_walk:
            continue  # an old directory's mtime moves when a child is removed
        if written(d_path, mtime) and not _is_cache(d_path):
            if os.path.isdir(d_path) and any(
                k.startswith(d_path + os.sep) for k in after_walk
            ):
                continue  # report the files, not the directory holding them
            found.add(d_path)

    for rel, _status in after_git.items():
        full = root / rel
        if _is_cache(rel) or any(str(full).startswith(s) for s in skip):
            continue
        if full.is_dir():
            if rel in before_git:
                continue  # an existing ignored directory is not ours to read
            walked = _walk(full)
            files = [
                p
                for p in walked
                if not os.path.isdir(p)
                and not _is_cache(os.path.relpath(p, root))
            ]
            found.update(files)
            if not os.listdir(full):  # created and left empty
                found.add(str(full))
            continue
        try:
            mtime = full.lstat().st_mtime_ns
        except OSError:
            continue
        if rel not in before_git or mtime >= start_ns:
            found.add(str(full))
    return sorted(
        os.path.relpath(p, root) if p.startswith(str(root) + os.sep) else p
        for p in found
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument(
        "--ctest",
        action="store_true",
        help="CMD is ctest: also walk every test's working dir",
    )
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    args = ap.parse_args(argv)
    cmd = args.cmd[1:] if args.cmd[:1] == ["--"] else args.cmd
    if not cmd:
        ap.error("no command given (put it after --)")
    root = args.root.resolve()

    dirs = _ctest_dirs(cmd) if args.ctest else {}
    if args.ctest and not dirs:
        print(
            "check-test-leaks: ctest listed no test working directories;"
            " refusing to report a clean run it did not check",
            file=sys.stderr,
        )
        return 1
    roots = _roots(dirs)
    before_git = _git_entries(root)
    before_walk: dict[str, int] = {}
    for r in roots:
        before_walk.update(_walk(Path(r)))

    start_ns = time.time_ns()
    rc = subprocess.call(cmd)
    if rc != 0:
        return rc

    after_walk: dict[str, int] = {}
    for r in roots:
        after_walk.update(_walk(Path(r)))
    left = leaks(
        root,
        before_git,
        _git_entries(root),
        before_walk,
        after_walk,
        start_ns,
        roots,
    )
    what = f"{len(roots)} ctest working tree(s) + " if roots else ""
    if not left:
        print(
            f"check-test-leaks: OK -- nothing left behind in {what}"
            f"{root.name}/"
        )
        return 0
    print(
        f"check-test-leaks: FAIL -- a passing run left {len(left)} "
        "file(s) behind:",
        file=sys.stderr,
    )
    for p in left:
        full = p if os.path.isabs(p) else str(root / p)
        print(f"  {p}{_owner(os.path.realpath(full), dirs)}", file=sys.stderr)
    print(
        "A test must remove what it writes -- work in a scratch directory"
        " (test_wfm_reader_core.c) or delete each file.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
