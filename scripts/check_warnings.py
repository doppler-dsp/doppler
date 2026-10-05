#!/usr/bin/env python3
"""Gate: doppler's own C builds clean under -Wall -Wextra, gcc AND clang.

This tree compiled with no warning flag at all: 0 of 691 compile lines
carried `-Wall`, so an unused result, a sign compare, a fallthrough or an
unhandled enum was invisible to every gate except the doc-snippet compiles
(doppler#1658). Measuring it found three real defects among the warnings in
doppler's own C (a `calloc` sized from an unset `n` at twenty sites, an
argmax read before it was set, a state header read after a read that wrote
nothing). Fixing them left that C at zero -- and nothing to keep it there.
**This is the gate.**

`make warnings-check` builds the whole tree (libraries, tests, benchmarks,
examples, Python extensions) in a fresh Release tree under each compiler with
`-Wall -Wextra`, and hands each build's log and compilation database here.

What it enforces, per warning (unique `file:line:flag`):

- a warning in a file under `vendor/` is not ours and is counted, not judged;
- a warning in a file on the EXEMPT list is tolerated -- see below;
- a warning anywhere else FAILS. There is no allowance and no count: new
  code fits, or the cause is fixed. That includes a freshly generated jm
  fragment, which is why jm's 0.98.1 template fix is worth having.

**The exempt list is a RATCHET and it only shrinks.** The 99 hand-owned
`<mod>_ext_<obj>.c` fragments plus `stream_ext.c` carry what is left of the
CPython glue's warnings (the unused `args`/`kwds` on `tp_new`, the one-field
`{ NULL }` sentinels, the `PyCFunction` casts). `jm apply` reconciles a
fragment member by member and never re-renders it, so no pin bump clears
them -- only the fragment migration does (doppler#1446). Until a fragment is
regenerated it stays on `scripts/.warnings-exempt`; when it is, its entry
must come OFF (a listed file that warns under neither compiler is a waiver
outliving its reason, so the gate fails on it) and no entry may be ADDED:
`--exempt-only` compares the list with the merge base and fails on a new line.
Nothing counts WITHIN an exempt file, deliberately: counts differ with
compiler versions, and the list is what the migration retires.

The file is named by what the COMPILER reports, and a compiler reports a
warning against the file that contains it. That is why the fragments can be
exempted individually although the aggregator `<mod>_ext.c` that
`#include`s them is the translation unit.

**Both compilers, because they disagree.** The gcc-only findings were all
`-Wmaybe-uninitialized`; the clang-only ones `-Wmissing-field-initializers`.
**Release**, because `-Wmaybe-uninitialized` needs the optimiser's flow
analysis. A gate that ran one compiler, or Debug, would pass on the other.

**It is a gate over the build log, not a flag in CMakeLists.txt.** A flag
could not exempt one source file in a subdirectory on CMake 3.16 (the
`TARGET_DIRECTORY` source property is 3.18), and a bare `-Werror` in the build
would turn every user's newer compiler into a build break.

A log alone proves nothing -- an empty one is what a build that never ran, or
ran without the flags, leaves behind. So the gate also reads each build's
`compile_commands.json` and fails unless every in-tree translation unit
carries both flags, and unless the log shows each of them being compiled.

Usage:
    python3 scripts/check_warnings.py \\
        --compiler gcc build-warnings-gcc.log build-warnings-gcc \\
        --compiler clang build-warnings-clang.log build-warnings-clang
    python3 scripts/check_warnings.py --exempt-only [--base origin/main]

Run it with `make warnings-check`; the list's own integrity is
`make lint-warnings-exempt`, which `make lint` includes.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import sys
from pathlib import Path

from _gitbase import BaseUnreadableError, in_git_repo, show_at_base

ROOT = Path(__file__).resolve().parent.parent
EXEMPT = Path(__file__).parent / ".warnings-exempt"

#: Upstream code we vendor and do not edit. Counted, never judged.
VENDORED = "vendor/"

#: What every in-tree translation unit must have been compiled with.
FLAGS = ("-Wall", "-Wextra")

#: gcc and clang agree on the shape: `path:line:col: warning: text [-Wflag]`.
#: The message is locale-dependent, which is why `make warnings-check` pins
#: `LC_ALL=C` -- a translated "warning" would match nothing and read as clean.
WARNING = re.compile(
    r"^(?P<path>\S[^:\n]*):(?P<line>\d+):\d+: warning: (?P<msg>.*)$"
)
FLAG = re.compile(r"\[(-W[^\]]+)\]\s*$")

#: A driver-level warning has no source location (`clang: warning: overriding
#: '-ffast-math' ...`). It is reported, not judged: it is about how a target
#: was configured, not about a line of C, and there is nothing to exempt.
DRIVER = re.compile(r"^(?:clang|gcc|cc1|cc): warning: ")

#: One per translation unit, from `make` and `ninja` alike.
COMPILING = re.compile(r"Building C object ")


def parse_log(text: str) -> tuple[dict[tuple[str, int, str], str], list[str]]:
    """The unique warnings in a compiler log, and its driver warnings.

    Unique means `(path, line, flag)`: a header included by forty translation
    units warns forty times at one place, and a macro warns once per
    expansion on one line, so counting lines would measure the include graph.
    The first message seen for a key is kept, for the failure report.

    >>> log = (
    ...     "/r/a.c:3:9: warning: unused variable 'x' [-Wunused-variable]\\n"
    ...     "/r/a.c:3:9: warning: unused variable 'x' [-Wunused-variable]\\n"
    ...     "clang: warning: overriding '-ffast-math' [-Woverriding-option]\\n"
    ... )
    >>> found, driver = parse_log(log)
    >>> sorted(found)
    [('/r/a.c', 3, '-Wunused-variable')]
    >>> len(driver)
    1
    """
    found: dict[tuple[str, int, str], str] = {}
    driver: list[str] = []
    for raw in text.splitlines():
        if DRIVER.match(raw):
            driver.append(raw)
            continue
        m = WARNING.match(raw)
        if not m:
            continue
        flag = FLAG.search(m["msg"])
        key = (m["path"], int(m["line"]), flag[1] if flag else "(unflagged)")
        # The flag is the key's third part; leaving it on the message too
        # would print it twice in a failure report.
        found.setdefault(key, FLAG.sub("", m["msg"]).rstrip())
    return found, driver


def to_rel(path: str, root: Path, build: Path) -> str:
    """A path as the compiler printed it -> repo-relative, POSIX.

    An absolute path under `root` is made relative; one outside it (a system
    or Python header) is returned absolute, which can never match `vendor/`
    or the exempt list and so is judged like any other file of ours -- a
    warning raised from a header by OUR code is ours to fix. A relative path
    is resolved against the tree first, then the build directory.

    >>> to_rel("/r/native/a.c", Path("/r"), Path("/r/b"))
    'native/a.c'
    >>> to_rel("/usr/include/x.h", Path("/r"), Path("/r/b"))
    '/usr/include/x.h'
    """
    p = Path(path)
    if not p.is_absolute():
        p = root / p if (root / p).exists() else build / p
    p = Path(os.path.normpath(p))
    try:
        return p.relative_to(root).as_posix()
    except ValueError:
        return p.as_posix()


def load_exempt_text(text: str) -> list[str]:
    """`<path>` per line; `#` starts a comment; blank lines are ignored.

    >>> load_exempt_text("# why\\n\\na/b.c\\nc/d.c  # trailing\\n")
    ['a/b.c', 'c/d.c']
    """
    out = []
    for raw in text.splitlines():
        body = raw.partition("#")[0].strip()
        if body:
            out.append(body)
    return out


def check_database(build: Path, root: Path) -> tuple[int, list[str]]:
    """In-tree translation units in `build`'s database, and any without flags.

    "In-tree" is everything not under `vendor/` and not outside `root`. A
    gate that read warnings from a build made WITHOUT `-Wall -Wextra` would
    find none and pass, which is the failure this exists to refuse.
    """
    entries = json.loads((build / "compile_commands.json").read_text())
    units = 0
    lacking: list[str] = []
    for e in entries:
        f = Path(e["file"])
        if not f.is_absolute():
            f = Path(e["directory"]) / f
        rel = to_rel(str(f), root, build)
        if rel.startswith(VENDORED) or rel.startswith("/"):
            continue
        units += 1
        argv = e.get("arguments") or shlex.split(e["command"])
        if not all(flag in argv for flag in FLAGS):
            lacking.append(rel)
    return units, sorted(lacking)


def exempt_only(root: Path, exempt: Path, base: str) -> int:
    """The list's own integrity: every entry exists, and none was ADDED.

    The second half is the ratchet. A list that could be appended to would be
    an allowlist, and an allowlist is where a new warning goes to be
    forgiven. A branch that adds a line is told to fix the cause instead.
    """
    entries = load_exempt_text(exempt.read_text()) if exempt.exists() else []
    problems: list[str] = []
    dups = sorted({e for e in entries if entries.count(e) > 1})
    problems += [f"  {e}: listed more than once" for e in dups]
    problems += [
        f"  {e}: listed, but the file is gone -- remove the entry"
        for e in entries
        if not (root / e).is_file()
    ]
    rel = exempt.resolve().relative_to(root).as_posix()
    if in_git_repo(root):
        try:
            then = show_at_base(root, base, rel)
        except BaseUnreadableError:
            print(
                f"FAIL: cannot resolve {base}, so an ADDED exemption cannot\n"
                "  be told from an old one. A ratchet that cannot read its\n"
                "  baseline has not passed.\n"
                "  Fetch it:  git fetch --no-tags --depth=1 origin \\\n"
                "               +refs/heads/main:refs/remotes/origin/main"
            )
            return 1
        # None: the list is new on this branch (the PR that introduces it),
        # so there is no earlier list to have grown from.
        if then is not None:
            before = set(load_exempt_text(then))
            problems += [
                f"  {e}: ADDED -- the exempt list may only shrink"
                for e in entries
                if e not in before
            ]
    if problems:
        print(
            "FAIL: scripts/.warnings-exempt is a ratchet and may only "
            "shrink.\n\n"
            "A warning in a file that is not on it is fixed at its cause,\n"
            "not forgiven by a new line here. For a jm fragment, regenerate\n"
            "it (the fragment migration, doppler#1446) and DELETE its "
            "entry.\n"
        )
        print("\n".join(problems))
        return 1
    print(f"warnings exempt list: OK -- {len(entries)} file(s), none added")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--exempt", type=Path, default=None)
    ap.add_argument(
        "--compiler",
        nargs=3,
        action="append",
        default=[],
        metavar=("NAME", "LOG", "BUILD_DIR"),
        help="one compiler's build log and build directory (repeatable)",
    )
    ap.add_argument(
        "--exempt-only",
        action="store_true",
        help="check the exempt list's integrity and ratchet; no build needed",
    )
    ap.add_argument(
        "--base",
        default="origin/main",
        help="ref to compare the exempt list against, for an ADDED entry",
    )
    a = ap.parse_args()
    root = a.root.resolve()
    exempt_path = a.exempt or (root / EXEMPT.relative_to(ROOT))

    if a.exempt_only:
        return exempt_only(root, exempt_path, a.base)
    if not a.compiler:
        ap.error("name at least one --compiler, or pass --exempt-only")

    exempt = (
        load_exempt_text(exempt_path.read_text())
        if exempt_path.exists()
        else []
    )
    failures: list[str] = []
    summary: list[str] = []
    dirty: set[str] = set()
    own: dict[str, list[tuple[str, int, str, str]]] = {}
    driver_all: list[str] = []

    for name, log_arg, build_arg in a.compiler:
        log, build = Path(log_arg), Path(build_arg)
        text = log.read_text(errors="replace")
        units, lacking = check_database(build, root)
        compiled = len(COMPILING.findall(text))
        if units == 0:
            failures.append(
                f"  {name}: no in-tree translation unit in {build}"
                "/compile_commands.json -- nothing was measured"
            )
        if lacking:
            failures.append(
                f"  {name}: {len(lacking)} translation unit(s) compiled "
                f"WITHOUT {' '.join(FLAGS)}, so their warnings were never "
                f"asked for, e.g. {lacking[0]}"
            )
        if compiled < units:
            failures.append(
                f"  {name}: the log shows {compiled} compile(s) but the "
                f"database lists {units} in-tree unit(s) -- a stale or "
                "partial log has not measured the tree"
            )
        found, driver = parse_log(text)
        driver_all += [f"{name}: {d}" for d in driver]
        n_vendored = n_exempt = n_own = 0
        for (path, line, flag), msg in sorted(found.items()):
            rel = to_rel(path, root, build)
            if rel.startswith(VENDORED):
                n_vendored += 1
            elif rel in exempt:
                n_exempt += 1
                dirty.add(rel)
            else:
                n_own += 1
                own.setdefault(rel, []).append((name, line, flag, msg))
        summary.append(
            f"  {name}: {units} translation units under "
            f"{' '.join(FLAGS)}, {len(found)} unique warning(s) -- "
            f"{n_own} own, {n_exempt} in exempt files, "
            f"{n_vendored} vendored"
        )

    if own:
        failures.append(
            "  a warning in doppler's own C, which has none:\n"
            + "\n".join(
                f"      {rel}:{line}  [{flag}] ({cc})  {msg}"
                for rel, ws in sorted(own.items())
                for cc, line, flag, msg in sorted(ws, key=lambda w: w[1])
            )
        )

    missing = [e for e in exempt if not (root / e).is_file()]
    if missing:
        failures.append(
            "  exempt entries whose file is gone (remove them):\n"
            + "\n".join(f"      {e}" for e in missing)
        )
    if len(a.compiler) >= 2:
        slack = sorted(set(exempt) - dirty - set(missing))
        if slack:
            failures.append(
                "  exempt files that warn under NEITHER compiler -- the "
                "list is a ratchet and has gone slack, so delete these "
                "entries:\n" + "\n".join(f"      {e}" for e in slack)
            )
        slack_note = ""
    else:
        slack_note = (
            "\n  (one compiler given: which exempt files have gone clean is "
            "judged only over both)"
        )

    print("\n".join(summary) + slack_note)
    if driver_all:
        print(
            f"  {len(driver_all)} driver warning(s), not gated (no source "
            "location):\n" + "\n".join(f"      {d}" for d in driver_all)
        )
    if failures:
        print(
            "\nFAIL: the tree does not build clean under "
            f"{' '.join(FLAGS)}.\n\n"
            "Fix the cause. Do NOT add the file to scripts/.warnings-exempt:\n"
            "that list may only shrink (see this script's docstring).\n"
        )
        print("\n".join(failures))
        return 1
    print(
        f"warnings gate: OK -- no warning outside {len(exempt)} exempt "
        f"file(s) and {VENDORED}, under {len(a.compiler)} compiler(s)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
