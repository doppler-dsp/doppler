#!/usr/bin/env python3
"""Every markdown table in a generated validation report must be well formed.

Why this is not covered by ``make validate-check``
--------------------------------------------------
``validate-check`` re-runs each ``validate.py`` with ``--check`` and fails if
the committed ``results.md`` differs from what the generator produces now.
That is a **staleness** gate, and staleness is not correctness: a generator
that emits broken markdown agrees with itself perfectly, so the report is
"up to date" and wrong at the same time.

That is exactly how two shipped reports came to carry a malformed table.
``Report.table`` joined raw cell text with ``|``, and a DSP report reaches
for a magnitude constantly:

* ``src/doppler/agc/.../results.md`` -- the header cell
  ``worst |gain_db| over 4000 on-target samples`` parsed as **four** columns
  against a **two**-column body.
* ``src/doppler/util/.../results.md`` -- ``|per-sample - chunked|`` parsed as
  **five** against **three**.

Both render as broken tables on the docs site and in the GitHub blob view.
``Report.table`` now escapes every cell; this gate is the half that proves it
stayed fixed, and it reads the *rendered* file, so it also catches a report
edited by hand and a future generator that grows its own table emitter.

What it checks
--------------
For each pipe table found in each report: the delimiter row's column count
must equal the header's, and every body row must match too. Column count is
taken after removing escaped pipes (``\\|``), which is what a markdown
renderer does.

Discovered, not registered
--------------------------
Reports are found by glob (``src/doppler/*/tests/validation/*/results.md``),
so an object certified tomorrow is gated the moment its report exists, with
no list here to update -- the same rule the validation log itself follows.

Usage
-----
::

    python scripts/check_validation_tables.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GLOB = "src/doppler/*/tests/validation/*/results.md"

# A delimiter row: | --- | :--- | ---: | etc. Its presence is what turns the
# preceding line into a table header, per GitHub-flavoured markdown.
DELIM_RE = re.compile(r"^\s*\|?\s*:?-{1,}:?\s*(\|\s*:?-{1,}:?\s*)*\|?\s*$")


def columns(line: str) -> int:
    """Count a pipe row's cells the way a renderer does.

    Escaped pipes are content, not structure, so they are removed before
    counting. Leading and trailing delimiters are stripped -- ``| a | b |``
    and ``a | b`` are both two columns.
    """
    stripped = line.replace(r"\|", "")
    stripped = stripped.strip()
    if stripped.startswith("|"):
        stripped = stripped[1:]
    if stripped.endswith("|"):
        stripped = stripped[:-1]
    return len(stripped.split("|"))


def check(path: Path) -> list[str]:
    """Return one message per malformed table row in ``path``."""
    bad: list[str] = []
    lines = path.read_text().split("\n")
    rel = path.relative_to(ROOT)
    i = 0
    while i < len(lines) - 1:
        header, delim = lines[i], lines[i + 1]
        if "|" in header and DELIM_RE.match(delim) and "|" in delim:
            want = columns(header)
            if columns(delim) != want:
                bad.append(
                    f"{rel}:{i + 2}: delimiter row has "
                    f"{columns(delim)} columns, header has {want}"
                )
            j = i + 2
            while j < len(lines) and lines[j].strip().startswith("|"):
                if columns(lines[j]) != want:
                    bad.append(
                        f"{rel}:{j + 1}: row has {columns(lines[j])} "
                        f"columns, header has {want}"
                    )
                j += 1
            i = j
        else:
            i += 1
    return bad


def main() -> int:
    reports = sorted(ROOT.glob(GLOB))
    if not reports:
        # Vacuity guard: a glob that matches nothing would pass silently and
        # this gate would be decoration. The tree always has reports.
        print(
            "check_validation_tables: no report matched "
            f"{GLOB} — the gate would be vacuous",
            file=sys.stderr,
        )
        return 1

    bad: list[str] = []
    for r in reports:
        bad += check(r)

    if bad:
        print("check_validation_tables: malformed table(s):", file=sys.stderr)
        for b in bad:
            print(f"  {b}", file=sys.stderr)
        print(
            "\n  A cell containing '|' (a magnitude, say) must be escaped as "
            "'\\|'.\n  Report.table() does this for every cell — if this "
            "fired, something\n  bypassed it. See "
            "src/doppler/tests/_validation_common.py.",
            file=sys.stderr,
        )
        return 1

    print(
        f"check_validation_tables: OK — {len(reports)} report(s), "
        "every table well formed"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
