#!/usr/bin/env python3
"""A generated validation report must actually say what it claims to say.

Two structural properties, both of which a shipped report violated, and
neither of which any existing gate could see:

1. **Every markdown table parses.** Five of six reports had a malformed one.
2. **Every finding it counts, it shows.** All six headed section 3
   "Findings, with verdicts" and rendered NONE of them -- 50 findings that
   existed only in console output, with the reports cross-referencing into
   the empty section ("recorded as §3 F6").

Both are checked against the RENDERED file, which is the point: they are
properties of the artifact a reader opens, not of the code that wrote it.

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


# "- **F1 · FIXED** — ..." as Report.find renders it.
FINDING_RE = re.compile(r"^- \*\*(F\d+) · ", re.M)
# Section 5's own count: "- **6 findings**, 0 of them gaps ..."
SUMMARY_RE = re.compile(r"^- \*\*(\d+) findings\*\*", re.M)


def check_findings(path: Path) -> list[str]:
    """The report must render every finding its summary counts.

    Both numbers come from the rendered file, so this is a self-consistency
    check: it needs no access to the validator, and it catches the exact
    defect that shipped -- a summary saying "6 findings" above a section 3
    containing none.
    """
    text = path.read_text()
    rel = path.relative_to(ROOT)
    claimed = SUMMARY_RE.search(text)
    if not claimed:
        return [f"{rel}: no '**N findings**' line in the summary"]
    want = int(claimed.group(1))
    got = FINDING_RE.findall(text)
    if len(got) != want:
        return [
            f"{rel}: the summary counts {want} finding(s) and the report "
            f"renders {len(got)} — section 3 is where they belong"
        ]
    return []


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
        bad += check_findings(r)

    if bad:
        print("check_validation_reports: FAIL", file=sys.stderr)
        for b in bad:
            print(f"  {b}", file=sys.stderr)
        print(
            "\n  A cell containing '|' (a magnitude, say) must be escaped as "
            "'\\|', and\n  every finding must be rendered where the report "
            "says it is. Report.table()\n  and Report.find() both do this "
            "already — if this fired, something bypassed\n  them. See "
            "src/doppler/tests/_validation_common.py.",
            file=sys.stderr,
        )
        return 1

    print(
        f"check_validation_reports: OK — {len(reports)} report(s), "
        "tables well formed, every finding rendered"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
