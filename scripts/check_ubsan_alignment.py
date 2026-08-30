#!/usr/bin/env python3
"""Hold the UBSan misaligned-access class to a ratchet, per FILE.

`make test-ubsan` used to exclude `alignment` with ``-fno-sanitize=`` and call
the exclusion a ratchet that "may only ever shrink". Nothing read it, and it
went 821 -> 853 -> 934 (doppler#1028). Excluding was also strictly weaker than
counting: an exclusion cannot see a *new* misalignment at all, only reports
outside the excluded class.

**Sites, not reports.** The first cut of this gate ratcheted the number of
reports. That number is not a property of the code: it is how many times the
suite happened to execute a misaligned access, and it moves with timing and
core count. Measured -- 933/934 across ten runs on one machine, and **1058**
on CI for the same commit. A ceiling on it is either machine-specific or so
loose it gates nothing.

What IS code-determined is which source locations do it. This counts the
distinct ``file:line:col`` sites and compares per FILE, so:

* a new misaligned cast anywhere fails, whatever the timing;
* moving a function within a file does not, because the key is the file and
  not the line -- the same reason the alloc-helper baseline counts per file
  (`check_alloc_helpers.py`), and the same hazard the arity ratchet's header
  calls out.

FEWER sites than the baseline is a WARNING, not a failure, and that is not a
courtesy -- it is what makes the baseline portable. Which sites execute
depends on which tests RUN, and that differs by environment: CI has a NATS
broker so `test_tlm_sink` runs there and produces four sites a developer box
never sees. Measured, after assuming otherwise and being corrected by CI: 18
sites across three files that appear on CI and not here.

So the baseline is seeded from CI, the environment that gates, and a machine
running less warns rather than fails. A new cast still fails wherever it
appears. Tighten the baseline deliberately when a fix lands.

Usage
-----
    python3 scripts/check_ubsan_alignment.py <ctest -V log> [--update]
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BASELINE = REPO / "native" / "tests" / ".ubsan-alignment-sites"

# A UBSan report line: `<path>:<line>:<col>: runtime error: <what>`.
_REPORT = re.compile(r"(\S+?):(\d+):(\d+): runtime error: (.*)$")
_MISALIGNED = "misaligned address"


def parse(log: Path) -> tuple[Counter, list[str]]:
    """``(sites-per-file, non-alignment report lines)`` from a ctest -V log."""
    sites: set[tuple[str, str, str]] = set()
    others: list[str] = []
    for raw in log.read_text(errors="replace").split("\n"):
        m = _REPORT.search(raw)
        if not m:
            continue
        path, line, col, what = m.groups()
        # Absolute in the log and machine-specific; keyed from `native/`.
        idx = path.find("native/")
        rel = path[idx:] if idx >= 0 else path
        if _MISALIGNED in what:
            sites.add((rel, line, col))
        else:
            others.append(f"{rel}:{line}:{col}: {what}")
    per_file = Counter(rel for rel, _, _ in sites)
    return per_file, others


def read_baseline(path: Path) -> dict[str, int] | None:
    """``{file: sites}``, or None when it cannot be read.

    None is a FAILURE and not an empty baseline: a ratchet that cannot read
    its own record has not checked anything.
    """
    if not path.exists():
        return None
    out: dict[str, int] = {}
    for raw in path.read_text().split("\n"):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        count, _, name = line.partition(" ")
        if not count.isdigit() or not name.strip():
            return None
        out[name.strip()] = int(count)
    return out or None


def write_baseline(path: Path, per_file: Counter, header: str) -> None:
    body = "\n".join(f"{n} {f}" for f, n in sorted(per_file.items()))
    path.write_text(header + body + "\n")


HEADER = """\
# Distinct source locations that produce a UBSan `member access within
# misaligned address` report, counted per FILE. Read by
# `scripts/check_ubsan_alignment.py`, which `make test-ubsan` runs.
#
# SITES, not reports. The number of reports is how many times the suite
# happened to execute one, which moves with timing and core count -- measured
# 933/934 across ten local runs and 1058 on CI for the same commit. The set of
# locations is a property of the code; the count of executions is not.
#
# Per FILE and not per line, so moving a function does not churn the record --
# the same choice `check_alloc_helpers.py` documents.
#
# RATCHETED: a file with MORE sites fails, and a file not listed here may have
# none. FEWER is a warning: a site not executed on some machine is not a site
# that was fixed, so tighten this deliberately when a fix lands
# (`--update`).
#
# Every one is a byte cursor cast to a struct pointer. Fixing them is its own
# change (doppler#1028).
#
"""


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("log", type=Path)
    ap.add_argument(
        "--update", action="store_true", help="rewrite the baseline"
    )
    a = ap.parse_args(argv)

    if not a.log.exists():
        print(
            f"check_ubsan_alignment: FAIL -- no log at {a.log}.\n"
            "  Nothing to read means nothing was checked.",
            file=sys.stderr,
        )
        return 1

    per_file, others = parse(a.log)

    if a.update:
        write_baseline(BASELINE, per_file, HEADER)
        total = sum(per_file.values())
        print(
            f"check_ubsan_alignment: wrote {total} site(s) across "
            f"{len(per_file)} file(s) to {BASELINE.name}"
        )
        return 0

    if others:
        print(
            f"check_ubsan_alignment: FAIL -- {len(others)} "
            "undefined-behaviour report(s) that are NOT the ratcheted "
            "alignment class:",
            file=sys.stderr,
        )
        for o in sorted(set(others))[:20]:
            print(f"    {o}", file=sys.stderr)
        return 1

    base = read_baseline(BASELINE)
    if base is None:
        print(
            f"check_ubsan_alignment: FAIL -- cannot read {BASELINE}.\n"
            "  A ratchet that cannot read its own record has not checked "
            "anything.",
            file=sys.stderr,
        )
        return 1

    # An empty parse is not a pass: the run produced no reports at all, which
    # on a tree with a non-empty baseline means the log is wrong, not the code.
    if not per_file and base:
        print(
            "check_ubsan_alignment: FAIL -- the log carries no alignment "
            "report at all,\n  but the baseline lists "
            f"{sum(base.values())}. Either the run did not instrument the "
            "class,\n  or the log was captured without `ctest -V` -- with "
            "reports non-fatal every\n  test PASSES and "
            "`--output-on-failure` prints nothing.",
            file=sys.stderr,
        )
        return 1

    grew = {
        f: (base.get(f, 0), n)
        for f, n in per_file.items()
        if n > base.get(f, 0)
    }
    shrank = {
        f: (base[f], per_file.get(f, 0))
        for f in base
        if per_file.get(f, 0) < base[f]
    }

    if grew:
        print(
            f"check_ubsan_alignment: FAIL -- {len(grew)} file(s) have MORE "
            "misaligned sites than the baseline:",
            file=sys.stderr,
        )
        for f, (was, now) in sorted(grew.items()):
            print(f"    {f}: {was} -> {now}", file=sys.stderr)
        print(
            "\n  A new byte-cursor cast. Fix it, or say why the baseline "
            "moves and by how much.",
            file=sys.stderr,
        )
        # The FULL observed table, not just the rows that grew. Which sites
        # execute depends on which tests run, and that differs by
        # environment -- CI runs `test_tlm_sink` (it has a NATS broker) and
        # a developer box may not. Printing everything means one failing run
        # carries the whole baseline, instead of a round trip per row.
        print("\n  observed, in full:", file=sys.stderr)
        for f, n in sorted(per_file.items()):
            print(f"    {n} {f}", file=sys.stderr)
        return 1

    for f, (was, now) in sorted(shrank.items()):
        print(f"  note: {f}: {was} -> {now} site(s)")
    if shrank:
        print(
            "  Fewer sites than the baseline. If a fix landed, tighten it:\n"
            "    make test-ubsan-baseline"
        )

    total = sum(per_file.values())
    print(
        f"check_ubsan_alignment: OK — {total} misaligned site(s) across "
        f"{len(per_file)} file(s), none above baseline; 0 report(s) outside "
        "the alignment class"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
