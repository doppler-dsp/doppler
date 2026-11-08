#!/usr/bin/env python3
"""Gate: a C file includes a header once per preprocessor branch.

`native/inc/doppler/burst_capture/burst_capture_core.h` included
`correlator_core.h` three times in a row, and three more headers
(`dsss_burst_receiver_core.h`, `async_dsss_pool_core.h`,
`async_dsss_receiver_core.h`) carried the same defect, found by review in
step 4c of gh-2752 and by nothing else. The include guard makes the
copies harmless to the compiler, so no build, test or warning could notice
them; they are noise a reader has to rule out, and the sign that an edit
landed somewhere it was not reviewed.

The rule
--------
Two `#include` lines name the same file when their targets are equal as
text after normalizing whitespace: `#include  "a.h"` and `#  include "a.h"`
are one include, while `"a.h"` and `<a.h>` stay distinct (they search
different paths). Comments are blanked first, so an include quoted in a
comment is not an include.

A repeat is flagged only when the earlier include is in effect wherever
the later one is. The scanner keeps a stack of open conditionals: each
`#if`/`#ifdef`/`#ifndef` pushes a frame, `#elif`/`#else` move that frame to
its next branch, and `#endif` pops it. An include's NESTING PATH is the
list of (conditional, branch) pairs it sits under. A later include repeats
an earlier one when the earlier one's path is a prefix of the later one's:
the same branch of the same conditionals, or an unconditional include
followed by the same header inside an `#if`. The same header in the `#if`
and the `#else` of one conditional is legitimate -- only one branch is ever
compiled -- and so is an include inside an `#if` repeated after its
`#endif`, which a header must do when the conditional copy may be absent.
The include guard wrapping a whole header is one more frame every include
shares, so it changes nothing.

Every tracked `.c` and `.h` is scanned except `vendor/**`, which is not
doppler's to fix. Generated C is scanned too: a repeat there is a codegen
defect, fixed where it is generated from. No allowlist -- every repeat was
removed when the gate landed, so the first occurrence fails.

Usage:  python3 scripts/check_dup_includes.py [--root DIR]
Exit 0 when no file repeats an include.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

from _c_source import strip_comments
from _gitbase import in_git_repo

ROOT = Path(__file__).resolve().parent.parent
SKIP_PARTS = {"build", "vendor", ".venv", "node_modules", ".git"}
DIRECTIVE = re.compile(r"^\s*#\s*(\w+)\s*(.*)$")
TARGET = re.compile(r'^(<[^>]*>|"[^"]*")')


def _files(root: Path) -> list[Path]:
    """Tracked C sources and headers, or every one under a synthetic tree.

    A gate's test harness seeds a directory that is not a repository, so
    the walk stands in for `git ls-files` there; it skips the same `vendor/`
    and the build trees a real checkout would not track.
    """
    if in_git_repo(root):
        out = subprocess.run(
            ["git", "ls-files", "-z", "--", "*.c", "*.h"],
            cwd=root,
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        rels = [r for r in out.split("\0") if r]
        paths = [root / r for r in rels]
    else:
        paths = [
            p
            for p in sorted(root.rglob("*"))
            if p.suffix in (".c", ".h")
            and not SKIP_PARTS.intersection(p.relative_to(root).parts[:-1])
        ]
    return [
        p
        for p in paths
        if "vendor" not in p.relative_to(root).parts[:-1] and p.is_file()
    ]


def _normalize(target: str) -> str:
    """`" a.h "` and `"a.h"` are one target; the delimiters are kept."""
    return target[0] + target[1:-1].strip() + target[-1]


def repeats(text: str) -> list[tuple[int, int, str]]:
    """(first line, repeat line, target) for each repeated include.

    >>> repeats('#include "a.h"\\n#  include  "a.h"\\n')
    [(1, 2, '"a.h"')]
    >>> repeats('#if X\\n#include "a.h"\\n#else\\n#include "a.h"\\n#endif\\n')
    []
    >>> repeats("#include <a.h>\\n#ifdef Y\\n#include <a.h>\\n#endif\\n")
    [(1, 3, '<a.h>')]
    >>> repeats('#include "a.h"\\n#include <a.h>\\n/* #include "a.h" */\\n')
    []
    """
    # Each frame is [conditional's line, branch index]; a path is the
    # tuple of frames, so equal tuples mean the same branch of the same
    # conditionals.
    stack: list[tuple[int, int]] = []
    seen: dict[str, list[tuple[tuple[tuple[int, int], ...], int]]] = {}
    found: list[tuple[int, int, str]] = []
    for n, line in enumerate(strip_comments(text).splitlines(), start=1):
        m = DIRECTIVE.match(line)
        if not m:
            continue
        word, rest = m.group(1), m.group(2).strip()
        if word in ("if", "ifdef", "ifndef"):
            stack.append((n, 0))
        elif word in ("elif", "else", "elifdef", "elifndef") and stack:
            line_no, branch = stack.pop()
            stack.append((line_no, branch + 1))
        elif word == "endif" and stack:
            stack.pop()
        elif word == "include":
            t = TARGET.match(rest)
            if not t:  # a macro-named include cannot be compared as text
                continue
            target = _normalize(t.group(1))
            path = tuple(stack)
            for earlier, first in seen.get(target, []):
                if path[: len(earlier)] == earlier:
                    found.append((first, n, target))
                    break
            else:
                seen.setdefault(target, []).append((path, n))
    return found


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args().root.resolve()

    found: list[tuple[str, int, int, str]] = []
    for path in _files(root):
        rel = path.relative_to(root).as_posix()
        for first, again, target in repeats(path.read_text(errors="replace")):
            found.append((rel, first, again, target))
    if not found:
        print("dup-include: no C file includes a header twice in one branch")
        return 0
    print("dup-include: a header is included again where it already is --")
    for rel, first, again, target in found:
        print(f"  {rel}:{again}: #include {target} (first at line {first})")
    print("  Delete the repeated #include line; the first one already holds.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
