#!/usr/bin/env python3
"""A generated validation report must actually say what it claims to say.

Three structural properties, every one of which a shipped report violated,
and none of which any existing gate could see:

1. **Every markdown table parses.** Five of six reports had a malformed one.
2. **Every finding it counts, it shows.** All six headed section 3
   "Findings, with verdicts" and rendered NONE of them -- 50 findings that
   existed only in console output, with the reports cross-referencing into
   the empty section ("recorded as §3 F6").
3. **Every figure it embeds exists, and every artifact beside it is
   cited.** RateSync's report grew two measurements -- the roll-off sweep
   that localised F15 and the amplitude law behind F13 -- with no figure and
   no CSV at all, while two of its existing figures quietly drew one
   detector under two-detector tables.

All three are checked against the RENDERED file, which is the point: they
are properties of the artifact a reader opens, not of the code that wrote
it.

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
**Tables** -- for each pipe table found in each report: the delimiter row's
column count must equal the header's, and every body row must match too.
Column count is taken after removing escaped pipes (``\\|``), which is what
a markdown renderer does.

**Findings** -- the count section 5 claims must equal the number section 3
renders.

**Artifacts** -- every embedded figure resolves, every figure in the folder
is embedded, and every ``data/*.csv`` is named in the text. Reachability
only: see ``check_artifacts`` for why this is not a pixel comparison.

Discovered, not registered
--------------------------
Reports are found by glob (``src/doppler/*/tests/validation/*/results.md``),
so an object certified tomorrow is gated the moment its report exists, with
no list here to update -- the same rule the validation log itself follows.

Usage
-----
::

    python scripts/check_validation_reports.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Two shapes, because a component with no Python face is certified by a C
# harness rendered by a validator under `src/doppler/tests/validation/`
# (docs/dev/contributing/validation.md, "certifying a component with no
# binding"). The
# first pattern is the per-module one every bound object uses; the second is
# the tree-wide home. Both are globs rather than a list, so a new object is
# gated the moment its folder exists.
GLOBS = (
    "src/doppler/*/tests/validation/*/results.md",
    "src/doppler/tests/validation/*/results.md",
)

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


#: The front matter, and the two things it must carry. Matched on the
#: RENDERED file so a report hand-edited into the wrong shape is caught
#: too, not just a generator that skipped the call.
EXEC_RE = re.compile(r"^## Executive summary\s*$", re.M)
STATUS_RE = re.compile(r"^- \*\*Status\.\*\* .+$", re.M)
TAKEAWAY_RE = re.compile(
    r"^\*\*What a caller most needs to know\*\*\s*$", re.M
)


def check_executive(path: Path) -> list[str]:
    """A report opens with an executive summary, and it opens with it.

    Every other section of a validation report is written for somebody
    already reading it. This one is for the person deciding whether to,
    and for the caller who will never read further than the status line —
    so it is the one section whose ABSENCE is invisible to every other
    gate here: the tables parse, the findings render, the figures resolve,
    and the report still fails to say whether the object is certified.

    Position is checked, not just presence. A summary that has drifted
    below section 1 is a summary nobody reads, and `Report.executive`
    renders into its own `head` list precisely so ordering cannot depend
    on call order. This catches a validator that emits the heading by hand
    instead.
    """
    text = path.read_text()
    rel = path.relative_to(ROOT)
    out: list[str] = []
    m = EXEC_RE.search(text)
    if not m:
        return [
            f"{rel}: no '## Executive summary' — the report never says "
            f"whether the object is certified. Call Report.executive() "
            f"from build(), after limits()"
        ]
    first = re.search(r"^## .+$", text, re.M)
    if first and first.start() != m.start():
        out.append(
            f"{rel}: '## Executive summary' is not the first section — it "
            f"is written last and read FIRST"
        )
    if not STATUS_RE.search(text):
        out.append(f"{rel}: the executive summary has no '**Status.**' line")
    if not TAKEAWAY_RE.search(text):
        out.append(
            f"{rel}: the executive summary lists no takeaways — a status "
            f"line alone is a scoreboard, not a summary"
        )
    return out


# "![the amplitude law](amp_law.png)" as a section emits it.
IMG_RE = re.compile(r"!\[[^\]]*\]\(([^)\s]+)\)")


def check_artifacts(path: Path) -> list[str]:
    """Figures and raw sweeps must be reachable FROM the report, and used.

    The gap this closes is the one ``make validate-check`` structurally
    cannot: it re-renders the markdown and compares bytes, while ``plots()``
    and ``_csv()`` run only under ``write=True``. So neither gate has ever
    opened a PNG or a CSV, and an artifact can drift behind the table it
    illustrates -- or stop being written entirely -- with both green.

    Deliberately NOT a rendering comparison. Matplotlib output is not
    byte-reproducible across versions, backends or available fonts, so
    diffing images would fail for reasons that have nothing to do with the
    measurement. What is checked instead is reachability, in both
    directions, which needs no rendering and no matplotlib:

    * every ``![...](x.png)`` resolves to a file that is actually there --
      catches a section referencing a figure ``plots()`` never drew, which
      is what happens when a measurement is added and its figure is
      forgotten;
    * every ``*.png`` in the folder is embedded by the report -- catches the
      reverse, a figure drawn and then orphaned by an edit, which leaves a
      stale image in the tree that nothing renders and nobody notices;
    * every ``data/*.csv`` is named in the text -- the raw sweeps exist so a
      reader can re-derive any number in a table, which they cannot do if
      the report never says the file is there.

    All three read the RENDERED report, so a hand-edited file is covered as
    well as a generated one.
    """
    folder = path.parent
    text = path.read_text()
    rel = path.relative_to(ROOT)
    bad: list[str] = []

    embedded: set[str] = set()
    for m in IMG_RE.finditer(text):
        target = m.group(1).strip()
        if target.startswith(("http://", "https://", "data:")):
            continue
        embedded.add(target)
        if not (folder / target).exists():
            bad.append(
                f"{rel}: embeds '{target}', which is not in the report's "
                f"folder — the section references a figure plots() does "
                f"not draw"
            )

    for png in sorted(folder.glob("*.png")):
        if png.name not in embedded:
            bad.append(
                f"{rel}: '{png.name}' sits beside the report and nothing "
                f"embeds it — either add the '![...]({png.name})' to the "
                f"section it illustrates, or delete the figure"
            )

    for csv in sorted(folder.glob("data/*.csv")):
        if f"data/{csv.name}" not in text and csv.name not in text:
            bad.append(
                f"{rel}: 'data/{csv.name}' is written and never cited — a "
                f"raw sweep exists so a reader can re-derive a table, which "
                f"needs the report to say it is there"
            )

    return bad


def check_lifecycle(path: Path) -> list[str]:
    """A certified object must have the deliverables the lifecycle names.

    ``docs/dev/contributing/adding-algorithms.md`` says what each phase
    *produces*, and
    nothing checked that any of it existed. That is not hypothetical: this
    gate was written after an audit found ``lockdet`` certified with 22
    holding limits, ``make gates`` all-pass -- and its phase-1 design page
    silent on a behaviour the certification had just changed. Every gate
    was green because no gate asked the question.

    Two things are checked, and both are DERIVED from the tree rather than
    listed here, so a new object is covered the day its report lands:

    * **the design page exists and is reachable from the report.** Section
      1 is required to *link* ``docs/design/<algo>.md`` rather than restate
      it, which only means anything if the link resolves. Catches an object
      certified with no design page at all, and a link left behind by a
      rename.
    * **some C test exercises the object.** Phase 4 is the evidence phase 8
      is built on, so a report with no C test underneath it is a report
      measuring something nothing pins. Matched on the object's core
      header or its ``<obj>_init`` / ``<obj>_step`` entry points rather
      than on a ``test_<obj>_core.c`` filename, because the filename
      convention does not hold: ``ema`` is a function primitive whose
      evidence lives in ``test_util_core.c``, and it is no less certified
      for that.

    What is deliberately NOT checked here is phase 9's *example*, which is
    the deliverable the audit actually found missing. Detecting it needs an
    object -> Python class mapping that the tree does not carry: the
    validation folder is named for the C core (``resamp``) while the
    binding is declared under another name (``Resampler``), with no link
    between them, and guessing at CamelCase would pass ``lockdet`` while
    failing ``resamp`` for no reason a reader could act on. A gate that is
    wrong about which objects comply is worse than no gate. Making it
    derivable means having the report cite its own example, the way it
    already cites its figures and CSVs -- filed as gh-744 rather than
    bodged, so this gate does not read as though it covers phase 9.
    """
    bad: list[str] = []
    rel = path.relative_to(ROOT)
    obj = path.parent.name
    text = path.read_text(encoding="utf-8")

    designs = dict.fromkeys(
        re.findall(r"\((?:\.\./)+(docs/design/[A-Za-z0-9_.-]+\.md)\)", text)
    )
    if not designs:
        bad.append(
            f"{rel}: section 1 links no docs/design page — the lifecycle's "
            "phase 1, and what section 1 is supposed to link rather than "
            "restate"
        )
    for d in designs:
        if not (ROOT / d).is_file():
            bad.append(f"{rel}: links '{d}', which does not exist")

    # An `#include "<obj>/..."` from anywhere under native/inc/<obj>/, or a
    # call to a symbol the object prefixes. The narrower `<obj>_core.h` this
    # used to require assumed every component's public header is named after
    # it that way, which held for thirteen objects and then did not:
    # `ccsds_tm` ships `ccsds_tm.h`, `ccsds_tm_rs.h` and `ccsds_tm_frame.h`,
    # is referenced by five C test files and 2500 lines of assertions, and
    # this gate reported it as pinned by nothing at all. A gate that is wrong
    # about which objects comply is the failure the docstring above already
    # names; a header NAME is not the fact being checked.
    pat = re.compile(
        rf'#include\s*"{re.escape(obj)}/|'
        rf"\b{re.escape(obj)}_[A-Za-z0-9_]*\s*\("
    )
    tests = ROOT / "native" / "tests"
    if not any(
        pat.search(t.read_text(encoding="utf-8", errors="ignore"))
        for t in tests.glob("*.c")
    ):
        bad.append(
            f"{rel}: no C test references '{obj}' — phase 8 is certifying "
            "an object that phase 4 never pinned"
        )
    return bad


def main() -> int:
    reports = sorted({p for g in GLOBS for p in ROOT.glob(g)})
    if not reports:
        # Vacuity guard: a glob that matches nothing would pass silently and
        # this gate would be decoration. The tree always has reports.
        print(
            "check_validation_tables: no report matched "
            f"{' or '.join(GLOBS)} — the gate would be vacuous",
            file=sys.stderr,
        )
        return 1

    bad: list[str] = []
    for r in reports:
        bad += check(r)
        bad += check_findings(r)
        bad += check_executive(r)
        bad += check_artifacts(r)
        bad += check_lifecycle(r)

    if bad:
        print("check_validation_reports: FAIL", file=sys.stderr)
        for b in bad:
            print(f"  {b}", file=sys.stderr)
        print(
            "\n  A cell containing '|' (a magnitude, say) must be escaped as "
            "'\\|', and\n  every finding must be rendered where the report "
            "says it is. Report.table()\n  and Report.find() both do this "
            "already — if this fired, something bypassed\n  them. See "
            "src/doppler/tests/_validation_common.py.\n\n  For a figure or "
            "a sweep: emit the '![...](x.png)' from the SECTION, never from"
            "\n  plots() — markdown written there is absent from the "
            "--check render, which\n  leaves validate-check permanently "
            "stale. See docs/dev/contributing/validation.md.",
            file=sys.stderr,
        )
        return 1

    print(
        f"check_validation_reports: OK — {len(reports)} report(s), "
        "tables well formed, every finding rendered, every figure and "
        "sweep reachable"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
