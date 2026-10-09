#!/usr/bin/env python3
"""Drop the test executables that SKIPPED from a coverage object list.

`make coverage` hands llvm-cov every instrumented `test_*`/`validate_*`
executable as an ``-object``, because a test binary carries coverage
mappings the library does not (a header's ``static`` helpers, compiled once
per translation unit). Each object contributes its mapped regions to the
report whether or not it ran -- and a test that SKIPPED ran nothing.

That is harmless for most tests: what a skipped binary maps is also mapped
by ``libdoppler.so``, so the lines are in the report either way. It is not
harmless for an instruction-set TIER. ``fir_extra.cmake`` compiles
``fir_core.c`` a second and third time, with ``-mavx2 -mfma`` and with
``-mavx512f -mavx512dq``, into objects linked only into
``test_fir_chunk_avx2`` / ``test_fir_chunk_avx512``. The portable build
never compiles the AVX-512 body at all, so that tier binary is the ONLY
object mapping it -- and on a host without AVX-512 the test exits 77
(SKIPPED) and every line of the body reports zero hits.

So the patch-coverage gate's verdict depended on which CPU the runner drew.
Measured on doppler#1934: the same ``fir_core.c`` and the same test passed
the gate on #1923's run (the AVX-512 tier ran: lines 222-259 hit 11,328 to
254,694 times) and failed it at 67% on the batch's (tier skipped: every one
of those lines DA:0, 20 lines "missing").

The honest reading of a skipped tier is "not measurable on this host", not
"not covered". This script reads which tests skipped from ctest's own
records and removes their executables from the list, and says so on stderr,
so the report covers what this host could run and names what it left out.

Inputs, both written by the coverage recipe in the build directory:

- ``--junit``: ``ctest --output-junit``. A skipped test is a ``<testcase>``
  with a ``<skipped>`` child (``SKIP_RETURN_CODE`` was returned). A FAILED
  test is not dropped -- the recipe has already stopped on it.
- ``--tests``: ``ctest --show-only=json-v1``, which maps each test's NAME
  to its COMMAND. The two need not agree (`add_test(NAME a COMMAND b)`), so
  the executable is taken from the command, never guessed from the name.

Candidate objects arrive on stdin, one path per line; the kept ones are
written to stdout in the same order. Paths are compared after
``os.path.realpath``, so a relative candidate matches ctest's absolute
command.

Exits 2 when either input is missing or unreadable. A list that silently
passed everything through would put back exactly the failure this exists
to remove, and look like it had worked.

Examples
--------
A skipped test whose executable is not named after it (``add_test(NAME
t_skip COMMAND bin_skip)``) is dropped by its command:

>>> import json, os, tempfile
>>> d = tempfile.mkdtemp()
>>> ran, skip = os.path.join(d, "t_ran"), os.path.join(d, "bin_skip")
>>> junit = os.path.join(d, "j.xml")
>>> with open(junit, "w") as f:
...     _ = f.write(
...         '<testsuite><testcase name="t_ran" status="run"/>'
...         '<testcase name="t_skip" status="notrun">'
...         '<skipped message="SKIP_RETURN_CODE=77"/></testcase>'
...         "</testsuite>"
...     )
>>> tests = os.path.join(d, "t.json")
>>> listing = [
...     {"name": "t_ran", "command": [ran]},
...     {"name": "t_skip", "command": [skip]},
... ]
>>> with open(tests, "w") as f:
...     json.dump({"tests": listing}, f)
>>> skipped = skipped_executables(junit, tests)
>>> skipped == {os.path.realpath(skip)}
True
>>> keep([ran, skip], skipped) == [ran]
True
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterable


def skipped_executables(junit: str, tests: str) -> set[str]:
    """Real paths of the executables whose ctest test was SKIPPED.

    Parameters
    ----------
    junit : str
        Path to ``ctest --output-junit``'s XML.
    tests : str
        Path to ``ctest --show-only=json-v1``'s JSON.

    Returns
    -------
    set of str
        ``os.path.realpath`` of each skipped test's ``command[0]``. A test
        with no command (a fixture-only entry) contributes nothing.
    """
    root = ET.parse(junit).getroot()
    names = {
        tc.get("name")
        for tc in root.iter("testcase")
        if tc.find("skipped") is not None
    }
    with open(tests, encoding="utf-8") as f:
        listing = json.load(f)
    return {
        os.path.realpath(t["command"][0])
        for t in listing.get("tests", [])
        if t.get("name") in names and t.get("command")
    }


def keep(candidates: Iterable[str], skipped: set[str]) -> list[str]:
    """The candidates that are not skipped executables, in input order."""
    return [c for c in candidates if os.path.realpath(c) not in skipped]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--junit", required=True, help="ctest --output-junit")
    ap.add_argument("--tests", required=True, help="ctest --show-only=json-v1")
    args = ap.parse_args(argv)
    try:
        skipped = skipped_executables(args.junit, args.tests)
    except (OSError, ET.ParseError, ValueError, KeyError) as e:
        print(
            f"cov_skipped_objects: cannot read ctest's records: {e}",
            file=sys.stderr,
        )
        return 2
    candidates = [ln.strip() for ln in sys.stdin if ln.strip()]
    kept = keep(candidates, skipped)
    dropped = sorted(os.path.basename(c) for c in candidates if c not in kept)
    if dropped:
        print(
            "coverage: left out of the report, SKIPPED on this host "
            f"(their code could not run here): {', '.join(dropped)}",
            file=sys.stderr,
        )
    sys.stdout.write("".join(f"{c}\n" for c in kept))
    return 0


if __name__ == "__main__":
    sys.exit(main())
