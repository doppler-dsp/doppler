#!/usr/bin/env python3
"""Every instrumented ctest leg must exclude the ``sweep`` validators.

Why this gate exists
--------------------

The ``validate_*`` harnesses register their ``--check`` spot check as a ctest
entry. That check belongs on every PR and it is cheap in the ordinary Release
suite. Re-running it under **instrumentation** is what costs, and the cost is
not proportional: measured on one coverage build over 20 cores
(doppler#1292), the 34 ``sweep`` validators were **94.3% of the instrumented
ctest CPU** (4615.7 s of 4893.6 s), and ``validate_acq_surface_jitter`` alone
took 1261.7 s against a 1266.0 s leg -- the suite finishes when that single
test finishes, so adding cores cannot help.

Three of the four instrumented suites already knew this. ASan, UBSan and TSan
each pass ``$(SAN_EXCLUDE_SWEEP)``, added with their own measurement of a
~60x instrumentation tax. The **coverage** suite never picked it up, and
nothing in the tree could say so: the omission had no symptom until the job
began being cancelled at its 90-minute cap, at which point it presented as
flaky infrastructure rather than as a leg running work it did not need. It
blocked every open PR in the repository for a day.

So the property is not "the coverage recipe carries a flag" -- that is the
fix, not the rule. The rule is: **if a block builds with instrumentation and
runs ctest, that ctest must exclude the sweep label.** A fourth sanitizer
added tomorrow is covered without being registered anywhere, which is the
point; the previous shape of this knowledge was a comment in one recipe that
the next recipe did not read.

What counts as instrumented
---------------------------

A block is instrumented when its own text configures a build with
``-fsanitize=`` or ``-DDOPPLER_COVERAGE=ON``. Nothing is listed by name, so
the classification follows the makefile rather than a roster that goes stale.

What counts as excluding
------------------------

The literal ``-LE sweep``, or a make variable whose definition expands to it
(``$(SAN_EXCLUDE_SWEEP)``, ``$(COV_EXCLUDE_SWEEP)``). Resolving variables is
what lets each suite keep its own escape hatch (``SAN_SWEEP=1``,
``COV_SWEEP=1``) instead of hard-coding the flag at six call sites.

Usage
-----

``check_instrumented_sweep.py [makefile ...]`` -- with no arguments it reads
``Makefile`` and ``standard.mk`` from the repository root. Explicit arguments
are how the gate's own test drives it over seeded makefiles, so that the gate
is proven to go red rather than merely observed to be green.

Exit status is 0 when every instrumented ctest leg excludes the sweep label,
1 otherwise.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# A build configured with either of these is instrumented: every test in it
# runs under a sanitizer's or the profiler's runtime, at a large constant
# factor over Release.
INSTRUMENTED = (
    re.compile(r"-fsanitize="),
    re.compile(r"-DDOPPLER_COVERAGE=ON"),
)

# `ctest`, however it is spelled -- `$(CTEST)` is the variable the makefiles
# use so a version-suffixed binary can be substituted.
CTEST = re.compile(r"\$\(CTEST\)|(?<![\w-])ctest(?![\w-])")

# The label exclusion, and the shape of a variable reference that may carry
# it. `-LE` takes the label as the next argument, so both spellings appear.
LITERAL = re.compile(r"-LE\s+sweep\b")
VARREF = re.compile(r"\$\((\w+)\)")

# `NAME = value` / `NAME ?= value` / `NAME := value`, captured so a reference
# to NAME can be resolved back to its text.
ASSIGN = re.compile(r"^([A-Za-z_][\w]*)\s*[:?+]?=\s*(.*)$")

# A recipe line is TAB-indented; a target header is a non-indented line with
# a colon that is not a variable assignment. `define NAME` ... `endef` blocks
# hold the multi-line command bodies (COVERAGE_CMD is one).
TARGET = re.compile(r"^[A-Za-z0-9_./%$()-][^=]*:(?!=)")


class Block:
    """A contiguous run of makefile text that runs as one unit."""

    def __init__(self, name: str, start: int) -> None:
        self.name = name
        self.start = start
        self.lines: list[tuple[int, str]] = []

    @property
    def text(self) -> str:
        return "\n".join(t for _, t in self.lines)

    def is_instrumented(self, variables: dict[str, str]) -> bool:
        """Does this block configure a build under a sanitizer or profiler?

        The marker is routinely one level of indirection away: the sanitizer
        targets pass ``"-DCMAKE_C_FLAGS=$(TSAN_FLAGS)"``, and it is
        ``TSAN_FLAGS`` that holds ``-fsanitize=thread``. Scanning the recipe
        text alone therefore recognised only the coverage leg, which spells
        ``-DDOPPLER_COVERAGE=ON`` literally -- and a gate that finds one of
        four legs would have passed this repository on the day three of them
        were the ones already doing it right.
        """
        return any(
            p.search(expand(self.text, variables)) for p in INSTRUMENTED
        )

    def ctest_lines(self) -> list[tuple[int, str]]:
        """Logical lines invoking ctest, with continuations joined.

        A ctest invocation is routinely split across several physical lines
        with trailing backslashes, and the `-LE sweep` may sit on any of
        them -- so the flag must be looked for in the *logical* line, not
        the physical one. Missing that is how a first draft reported TSan,
        whose exclusion sits two continuations down from `$(CTEST)`.
        """
        joined: list[tuple[int, str]] = []
        buf, first = "", 0
        for num, text in self.lines:
            stripped = text.rstrip()
            # A comment is not a command. Skipped explicitly because these
            # recipes carry long prose ABOUT ctest directly above the call
            # they describe -- the first draft of this gate reported its own
            # explanatory comment as an unexcluded leg.
            if stripped.lstrip().startswith("#"):
                continue
            if not buf:
                first = num
            buf += " " + stripped.rstrip("\\")
            if not stripped.endswith("\\"):
                if CTEST.search(buf):
                    joined.append((first, buf.strip()))
                buf = ""
        if buf and CTEST.search(buf):
            joined.append((first, buf.strip()))
        return joined


def read_blocks(text: str) -> list[Block]:
    """Split makefile text into define-blocks and target recipes."""
    blocks: list[Block] = []
    current: Block | None = None
    in_define = False
    for num, line in enumerate(text.splitlines(), start=1):
        if in_define:
            assert current is not None
            if line.strip() == "endef":
                blocks.append(current)
                current, in_define = None, False
            else:
                current.lines.append((num, line))
            continue
        m = re.match(r"^define\s+(\S+)", line)
        if m:
            current, in_define = Block(m.group(1), num), True
            continue
        if line.startswith("\t"):
            if current is not None:
                current.lines.append((num, line))
            continue
        # A comment or a blank line does NOT end a rule in make -- the recipe
        # resumes at the next TAB-indented line. These recipes routinely put
        # several paragraphs of prose between two commands (test-tsan carries
        # one between its build and its ctest), and treating that as the end
        # of the block truncated all three sanitizer legs to their configure
        # step: the gate then found one leg of four and called it OK.
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        # A non-indented, non-continuation line ends any open recipe. A
        # target whose recipe has not started yet has no lines to inspect,
        # and a bare `.PHONY: x` target legitimately never gets one.
        if current is not None and not (
            current.lines and current.lines[-1][1].rstrip().endswith("\\")
        ):
            blocks.append(current)
            current = None
        if TARGET.match(line) and not ASSIGN.match(line):
            current = Block(line.split(":", 1)[0].strip(), num)
        elif current is not None:
            current.lines.append((num, line))
    if current is not None:
        blocks.append(current)
    return blocks


def assignments(text: str) -> dict[str, str]:
    """Every ``NAME = value`` in the file, for one-level-deep expansion."""
    out: dict[str, str] = {}
    for line in text.splitlines():
        if line.startswith("\t"):
            continue
        if (m := ASSIGN.match(line)) is not None:
            out.setdefault(m.group(1), m.group(2))
    return out


def expand(text: str, variables: dict[str, str], depth: int = 3) -> str:
    """Substitute ``$(NAME)`` from `variables`, a few levels deep.

    Deliberately not a make evaluator: it only has to bring a flag held one
    or two variables away into view. Unknown names are left alone, so a
    reference this cannot resolve simply fails to match a marker rather than
    erroring -- the conservative direction is to report nothing, and the
    empty-result guard in `check` is what stops that reading as a pass.
    """
    for _ in range(depth):
        expanded = VARREF.sub(
            lambda m: variables.get(m.group(1), m.group(0)), text
        )
        if expanded == text:
            break
        text = expanded
    return text


def sweep_vars(text: str) -> set[str]:
    """Names of make variables whose value carries the sweep exclusion."""
    return {
        m.group(1)
        for line in text.splitlines()
        if (m := ASSIGN.match(line)) and LITERAL.search(m.group(2))
    }


def excludes_sweep(command: str, names: set[str]) -> bool:
    if LITERAL.search(command):
        return True
    return any(ref in names for ref in VARREF.findall(command))


def check(paths: list[Path]) -> int:
    findings: list[str] = []
    checked = 0
    for path in paths:
        text = path.read_text(encoding="utf-8")
        names = sweep_vars(text)
        variables = assignments(text)
        for block in read_blocks(text):
            if not block.is_instrumented(variables):
                continue
            for num, command in block.ctest_lines():
                checked += 1
                if not excludes_sweep(command, names):
                    findings.append(
                        f"{path}:{num}: {block.name}: an instrumented ctest "
                        f"leg does not exclude the sweep validators\n"
                        f"    {command[:100]}\n"
                        f"    add -LE sweep, or a variable carrying it "
                        f"(SAN_EXCLUDE_SWEEP / COV_EXCLUDE_SWEEP)"
                    )
    if findings:
        print("instrumented sweep exclusion: FAIL")
        for f in findings:
            print(f"  {f}")
        return 1
    # An empty result set is not a pass. A parser that stopped recognising
    # recipes would report zero findings over zero legs and read as clean --
    # the same trap the glibc and tarball gates were caught by.
    if checked == 0:
        print(
            "instrumented sweep exclusion: FAIL — no instrumented ctest leg "
            "was found at all; the makefile parser has stopped matching"
        )
        return 1
    print(
        f"instrumented sweep exclusion: OK — {checked} instrumented ctest "
        f"leg(s), every one excludes the sweep validators"
    )
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
