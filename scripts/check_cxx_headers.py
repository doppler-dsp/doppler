#!/usr/bin/env python3
"""Every installed header must parse as C++, bar a list that only shrinks.

`native/inc/doppler/` is installed wholesale, and a C++ program is a
consumer like any other: it writes `#include "doppler/fir/fir_core.h"` and
expects a declaration it can call. For a third of the tree that include was
a compile error (doppler#3108), and nothing said so, because no C++
compiler ever read a doppler header: the build declares `LANGUAGES C` and
is meant to.

What crosses and what does not is written down once, in `dp_complex.h`
("From C++"). In short: the complex TYPES cross, because g++ and clang++
accept `float _Complex` as the C compiler's own type; the C99 complex
LIBRARY (`crealf`, `CMPLXF`, `I`) does not, so a function a header defines
with it is hidden from C++ or declared there as a prototype. This gate is
what holds a header to that after it has been converted, and what stops a
new one arriving broken.

What it runs
------------
One translation unit a header, containing only `#include "<header>"`,
through `-fsyntax-only` at `--std` (C++11 unless told otherwise: the oldest
standard a consumer has reported using, and the one that rejects a C99
hexadecimal float literal). The include is NOT wrapped in `extern "C"`: a
header that needs its caller to supply the linkage is itself the defect.

Every compiler named with `--cxx` must accept the header. g++ and clang++
disagree about real things (`__builtin_complex` exists in clang++ and not
in g++), so one of them passing proves nothing about the other.

A ratchet, and why this one is not absolute
-------------------------------------------
`scripts/.cxx-headers-exempt` lists the headers that do not parse yet, one
a line, each with its reason. It can only shrink:

- a header that fails and is not listed fails the gate;
- a listed header that now parses fails the gate too, until its line is
  deleted, so the list never claims more breakage than there is;
- a line with no reason, or naming a file that is not installed, fails.

The correct length is zero. It is a list rather than zero today because
the conversion is one PR a family of headers, not one PR.

A second list: headers that are not C++ headers at all
------------------------------------------------------
`scripts/.cxx-headers-not-cxx` names the installed headers no C or C++
consumer includes, so the parse is not asked of them. It is not a ratchet
and need not shrink, so what may go on it is decided mechanically, not by
judgment. An entry must be

- **private**: its name ends `_private.h`, the convention
  `check_installed_headers.py` already reads as "internals, never a
  caller's API"; or **Python-only**: it includes `Python.h`; and
- **not reached from a consumer's include**: no installed header that is
  off this list includes it. A private header a public one includes is
  installed for C users in all but name, so it has to parse.

An entry that fails either test, has no reason, names a header that is not
installed, or is on both lists fails the gate.

A missing compiler is not a pass: the gate exits 1 and says it did not run.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import pathlib
import re
import shutil
import subprocess
import sys

import check_installed_headers

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXEMPT = ROOT / "scripts" / ".cxx-headers-exempt"
NOT_CXX = ROOT / "scripts" / ".cxx-headers-not-cxx"

#: A preprocessor include, capturing the name between the delimiters.
_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)


def read_exempt(path: pathlib.Path) -> tuple[dict[str, str], list[str]]:
    """Parse the exempt list into ``{header: reason}`` and its own faults.

    A line is ``<path under the include root>  # <reason>``. Blank lines
    and lines starting with ``#`` are skipped. A line with no reason is a
    fault rather than an entry: an unexplained exemption is how a ratchet
    turns into an allowlist.
    """
    entries: dict[str, str] = {}
    faults: list[str] = []
    if not path.is_file():
        return entries, faults
    for n, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        name, _, reason = line.partition("#")
        name, reason = name.strip(), reason.strip()
        if not reason:
            faults.append(f"{path.name}:{n}: `{name}` has no reason")
        elif name in entries:
            faults.append(f"{path.name}:{n}: `{name}` is listed twice")
        else:
            entries[name] = reason
    return entries, faults


def includes(inc: pathlib.Path, names: list[str]) -> dict[str, set[str]]:
    """``{header: the installed headers it includes}``, for every header.

    An include resolves against the include root (`"doppler/x/y.h"`, the
    tree's own spelling) and then against the including header's directory;
    a system header resolves to neither and is dropped.
    """
    known = set(names)
    out: dict[str, set[str]] = {}
    for name in names:
        here = (inc / name).parent
        text = (inc / name).read_text(encoding="utf-8", errors="replace")
        got: set[str] = set()
        for inc_name in _INCLUDE.findall(text):
            if inc_name in known:
                got.add(inc_name)
                continue
            rel = (here / inc_name).resolve()
            if rel.is_file() and rel.is_relative_to(inc):
                cand = rel.relative_to(inc).as_posix()
                if cand in known:
                    got.add(cand)
        out[name] = got
    return out


def not_cxx_faults(
    inc: pathlib.Path,
    entries: dict[str, str],
    names: list[str],
    listing: str,
) -> list[str]:
    """Every way the not-C++ list claims a header it may not.

    See "A second list" in the module docstring: an entry must be private
    (`*_private.h`) or Python-only (includes `Python.h`), and no installed
    header off the list may include it.
    """
    faults: list[str] = []
    graph = includes(inc, names)
    for name in sorted(entries):
        if name not in graph:
            continue  # reported as "not an installed header" by the caller
        text = (inc / name).read_text(encoding="utf-8", errors="replace")
        private = name.endswith("_private.h")
        python = "Python.h" in _INCLUDE.findall(text)
        if not (private or python):
            faults.append(
                f"{listing}: `{name}` is neither private (`*_private.h`) "
                "nor Python-only (includes Python.h), so it is a header "
                "for C users and must parse as C++"
            )
        users = sorted(
            h for h, got in graph.items() if name in got and h not in entries
        )
        if users:
            faults.append(
                f"{listing}: `{name}` is included by {', '.join(users)}, "
                "so it is installed for C users and must parse as C++"
            )
    return faults


def first_error(cxx: str, std: str, inc: pathlib.Path, header: str) -> str:
    """The compiler's first error for one header, or ``""`` when it parses.

    The exit code decides; the text is only what gets reported. A compiler
    that fails without the word ``error`` (it crashed, it was killed) is
    still a failure, reported by its exit code.
    """
    proc = subprocess.run(
        [cxx, f"-std={std}", "-x", "c++", "-fsyntax-only", f"-I{inc}", "-"],
        input=f'#include "{header}"\n',
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode == 0:
        return ""
    for line in proc.stderr.splitlines():
        if "error" in line:
            return line.strip()
    return f"{cxx} exited {proc.returncode} with no diagnostic"


def main(argv: list[str] | None = None) -> int:
    """Run the sweep; 0 only when the tree and the exempt list agree."""
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("--inc", type=pathlib.Path, default=ROOT / "native/inc")
    ap.add_argument("--exempt", type=pathlib.Path, default=EXEMPT)
    ap.add_argument("--not-cxx", type=pathlib.Path, default=NOT_CXX)
    ap.add_argument("--std", default="c++11")
    ap.add_argument(
        "--cxx",
        action="append",
        help="a C++ compiler every header must pass; repeat for several "
        "(default: g++ and clang++)",
    )
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args(argv)
    compilers = args.cxx or ["g++", "clang++"]

    missing = [c for c in compilers if shutil.which(c) is None]
    if missing:
        print(
            "cxx-headers-check: no "
            + ", ".join(missing)
            + " on PATH -- this gate has not run, so it has not passed."
        )
        return 1

    inc = args.inc.resolve()
    names = [
        h.relative_to(inc).as_posix()
        for h in check_installed_headers.headers(inc)
        if h.suffix == ".h"
    ]
    if not names:
        print(f"cxx-headers-check: no headers under {inc} -- nothing ran.")
        return 1

    exempt, problems = read_exempt(args.exempt)
    not_cxx, faults = read_exempt(args.not_cxx)
    problems += faults
    for listing, entries in ((args.exempt, exempt), (args.not_cxx, not_cxx)):
        problems += [
            f"{listing.name}: `{name}` is not an installed header"
            for name in entries
            if name not in names
        ]
    problems += [
        f"`{name}` is on both {args.exempt.name} and {args.not_cxx.name}"
        for name in sorted(set(exempt) & set(not_cxx))
    ]
    problems += not_cxx_faults(inc, not_cxx, names, args.not_cxx.name)
    names_cxx = [n for n in names if n not in not_cxx]

    def check(name: str) -> tuple[str, str]:
        for cxx in compilers:
            err = first_error(cxx, args.std, inc, name)
            if err:
                return name, f"{cxx}: {err}"
        return name, ""

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        results = dict(pool.map(check, names_cxx))

    broken = {n: e for n, e in results.items() if e}
    for name in sorted(broken):
        if name not in exempt:
            problems.append(
                f"{name} does not parse as C++\n    {broken[name]}"
            )
    for name in sorted(exempt):
        if name in results and name not in broken:
            problems.append(
                f"{name} parses as C++ now: delete its line from "
                f"{args.exempt.name}"
            )

    passing = len(names_cxx) - len(broken)
    if problems:
        print(f"cxx-headers-check: FAIL ({len(problems)})")
        for p in problems:
            print(f"  {p}")
        return 1
    print(
        f"cxx-headers-check: OK -- {passing} of {len(names_cxx)} installed "
        f"headers parse as {args.std} under {' and '.join(compilers)}; "
        f"{len(broken)} exempt (the list only shrinks); {len(not_cxx)} "
        "not C++ headers (private or Python-only)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
