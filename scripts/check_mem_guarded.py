#!/usr/bin/env python3
"""Every memory-heavy command in the makefiles runs under the shared guard.

Why this gate exists
--------------------

A 16 GiB WSL VM does not fail when it runs out of memory: it dies, and takes
every process on it -- shells, editors, SSH sessions -- along with it. It did
so twice in three days (2026-10-01 05:12 and 2026-10-03 20:45), both times
the same pair: the docs build (``zensical``, 6.0 GiB resident) running
beside an xdist pytest run (a worker per core, 2-6 GiB between them). The
kernel log names them; neither alone comes near the ceiling.

``scripts/mem-guard.sh`` already existed, but only the coverage leg used it,
and its ceiling was per command -- two guarded commands could each stay under
their own 3/4-of-RAM cap and still sink the machine together. It now puts
every guarded command in ONE shared systemd slice and caps the slice, so
concurrent heavy jobs share a single budget and an overrun kills the largest
of them instead of the VM.

That only holds if the heavy commands are actually guarded, and a new one is
added by writing an ordinary recipe. So the rule here is the property, not a
roster: **a pytest invocation that runs xdist workers (``-n`` anything but
``0``), and any zensical invocation, must reach ``mem-guard.sh`` once its
make variables are expanded.** A new parallel suite is covered the day it is
written, without being registered anywhere.

Parsing is borrowed from ``check_instrumented_sweep.py`` -- its continuation
joining and variable expansion -- rather than written a second time.

Usage
-----

``check_mem_guarded.py [makefile ...]`` -- with no arguments it reads
``Makefile`` and ``standard.mk`` from the repository root, in that order, so
the repo's own definition of a variable wins over the vendored default.
Explicit arguments are how the gate's own test drives it over seeded
makefiles.

Exit status is 0 when every heavy command is guarded, 1 otherwise.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from check_instrumented_sweep import (
    ASSIGN,
    PYTEST,
    Block,
    assignments,
    expand,
)

GUARD = "mem-guard.sh"

# xdist's worker count, in either spelling: `auto`, `logical`, a count
# other than 0, or a make variable. `-n 0` is xdist switched off -- one
# process -- and is the documented PYTEST_ARGS override, so it is not heavy.
# The value is spelled out rather than "anything" because these recipes are
# full of shell `[ -n "$$x" ]` tests, which a looser pattern reads as xdist.
XDIST = re.compile(
    r"(?:^|\s)(?:-n\s*|--numprocesses[=\s]+)"
    r"(?:auto|logical|0*[1-9]\d*|\$\()(?![\w-])"
)
ZENSICAL = re.compile(r"(?<![\w./-])zensical(?![\w./-])")
# Any command at all, so a logical line can be filtered by the two above
# after expansion -- `$(ZENSICAL) build` only says zensical once expanded.
ANY = re.compile(r"\S")


def heavy(command: str) -> str | None:
    """Name the reason *command* is heavy, or None if it is not."""
    if PYTEST.search(command) and XDIST.search(command):
        return "parallel pytest"
    if ZENSICAL.search(command):
        return "zensical"
    return None


def check(paths: list[Path]) -> int:
    files = [(p, p.read_text(encoding="utf-8")) for p in paths]
    variables: dict[str, str] = {}
    for _, text in files:
        for name, value in assignments(text).items():
            variables.setdefault(name, value)
    findings: list[str] = []
    checked = 0
    for path, text in files:
        # The whole file as one block: a heavy command is as likely to sit
        # in a `X_CMD = ...` assignment spanning continuations as in a
        # recipe, and the joining is the same either way.
        lines = text.splitlines()
        whole = Block(path.name, 1)
        whole.lines = list(enumerate(lines, start=1))
        for num, line in whole.logical_lines(ANY):
            # A definition the repo has overridden never runs -- standard.mk's
            # vendored `ZENSICAL ?= ...` default is the case -- so only the
            # effective one is judged.
            # Compared on the PHYSICAL first line, which is all `assignments`
            # keeps; the joined logical line of a continued `X_CMD = ...`
            # would never equal it and would drop out unjudged.
            m = ASSIGN.match(lines[num - 1])
            if m and variables.get(m.group(1)) != m.group(2):
                continue
            command = expand(line, variables)
            why = heavy(command)
            if why is None:
                continue
            checked += 1
            if GUARD not in command:
                findings.append(
                    f"{path}:{num}: {why} runs outside the memory guard\n"
                    f"    {line[:100]}\n"
                    f"    prefix it with $(MEM_GUARD_CMD)"
                )
    if findings:
        print("memory guard: FAIL")
        for f in findings:
            print(f"  {f}")
        return 1
    # Zero matches is a parser that stopped matching, not a clean tree.
    if checked == 0:
        print(
            "memory guard: FAIL — no parallel pytest or zensical command was "
            "found at all; the makefile parser has stopped matching"
        )
        return 1
    print(f"memory guard: OK — {checked} heavy command(s), every one guarded")
    return 0


def main(argv: list[str]) -> int:
    if argv:
        paths = [Path(a) for a in argv]
    else:
        root = Path(__file__).resolve().parent.parent
        paths = [root / "Makefile", root / "standard.mk"]
    return check([p for p in paths if p.exists()])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
