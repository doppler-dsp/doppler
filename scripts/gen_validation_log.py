#!/usr/bin/env python3
"""Generate the validation log's table from the committed reports.

`docs/dev/contributing/validation.md` says HOW an object is certified. This
generator
fills the companion page that says WHICH objects are — one row per
certified object, with its finding and limit counts and a link to its
report.

Why this is generated rather than written
-----------------------------------------
A hand-maintained index of certified objects is the same shape as every
list this repo has had to delete: `GALLERY_SCRIPTS` frozen beside a
second list of PNG names, the example gate stuck at 23 entries while the
directory grew to 62, and the validation tree itself, which needed a
registration step nobody performed. A row here appears the moment
`src/doppler/<module>/tests/validation/<object>/results.md` exists, by
the same glob `make validate` uses, and there is nothing to remember.

Where the numbers come from, and why they can be trusted
--------------------------------------------------------
Not from prose. `Report.summary()` in
``src/doppler/tests/_validation_common.py`` renders the closing block of
every report in one fixed shape::

    - **9 findings**, 0 of them gaps or confirmed defects — none left
    - **18/18 limits** hold

so those two lines are machine-written and parsed back here. That makes
the freshness chain explicit and gate-backed at every link:

1. `make validate` regenerates each `results.md` from the C, through its
   own binding;
2. `make validate-check` fails if a committed report is stale;
3. this generator derives the table from those reports, and
   `--check` (in `make docs-drift-check`) fails if the table is stale.

So the log cannot claim a certification the reports do not show, and the
reports cannot drift from the code. What none of it proves is that a
limit is the RIGHT limit — that is the sabotage step in
`docs/dev/contributing/validation.md`, and no amount of regeneration
substitutes for
it.

A report whose summary does not parse is a hard error, never a blank
row: a silently-empty cell in a coverage table reads as "nothing to
report" when it means "the generator lost track".

Usage
-----
::

    python scripts/gen_validation_log.py --write   # rewrite the block
    python scripts/gen_validation_log.py --check   # exit 1 on drift
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "doppler"
PAGE = ROOT / "docs" / "dev" / "contributing" / "validation-log.md"
OBJECTS = ROOT / "objects"

# The same glob `make validate` uses for VALIDATORS, one directory over:
# discovery is by arrival, not by registration.
GLOBS = (
    "*/tests/validation/*/results.md",
    # A component with no Python face is certified from `src/doppler/tests/`
    # instead, because there is no module to sit beside -- see
    #  docs/dev/contributing/validation.md, "certifying a component with no
    # binding".
    "tests/validation/*/results.md",
)

START_MARKER = "<!-- validation-log:start -->"
END_MARKER = "<!-- validation-log:end -->"

BLOB = "https://github.com/doppler-dsp/doppler/blob/main"

# Both written by Report.summary(); see the module docstring.
FINDINGS_RE = re.compile(
    r"^- \*\*(\d+) findings\*\*, (\d+) of them gaps or confirmed defects"
    # summary() writes ": F3, F4, F6" when there are open findings and
    # " — none left" when there are not. Both tails are matched, and the
    # line must end after one of them: anchoring here is what turns a
    # future change to that sentence into a loud failure rather than a
    # silently-empty cell.
    r"(?:: (?P<ids>.+?)| — none left)?\s*$"
)
LIMITS_RE = re.compile(r"^- \*\*(\d+)/(\d+) limits\*\* hold")
TITLE_RE = re.compile(r"^#\s+(.*?)\s+—\s+validation report\s*$")


class ParseError(RuntimeError):
    """A report did not carry the machine-rendered summary block."""


def normalize(block: str) -> str:
    """A table's CONTENT, with the layout mdformat owns stripped out.

    `mdformat` (pre-commit) pads every table cell to its column width and
    stretches the `---` separator to match. Emitting that padding here
    would mean reimplementing another tool's column-width algorithm and
    keeping it in step forever; emitting it *unpadded* and comparing
    literally is worse still — the classic generator-versus-formatter
    loop, where `--write` is immediately re-dirtied by the formatter and
    the `--check` right after false-fails. Observed exactly that before
    this function existed.

    So the split is by ownership: the formatter decides how a row is
    laid out, this generator decides what is in it, and the comparison
    only ever looks at the latter. Runs of spaces collapse, cells are
    trimmed, and a separator row of any width reduces to one form.
    """
    out = []
    for line in block.splitlines():
        s = line.strip()
        if s.startswith("|"):
            cells = [c.strip() for c in s.strip("|").split("|")]
            # A separator row is all dashes-and-colons; its width is
            # pure layout, so every width folds to the same token.
            cells = ["---" if set(c) <= set("-:") and c else c for c in cells]
            s = "|" + "|".join(cells) + "|"
        out.append(s)
    return "\n".join(out)


def parse(report: Path) -> dict[str, object]:
    """Pull one row's worth of facts out of a committed report.

    Raises
    ------
    ParseError
        If the title or either summary line is missing. Refusing to
        guess is the point: a blank cell in a coverage table is read as
        "clean", which is the opposite of "unknown".
    """
    text = report.read_text(encoding="utf-8")
    title = findings = limits = None
    for line in text.splitlines():
        if title is None and (m := TITLE_RE.match(line)):
            title = m.group(1)
        elif findings is None and (m := FINDINGS_RE.match(line)):
            # The trailing ": F3, F4, F6" is present only when there ARE
            # open findings; summary() writes " — none left" otherwise.
            findings = (int(m.group(1)), int(m.group(2)), m.group("ids"))
        elif limits is None and (m := LIMITS_RE.match(line)):
            limits = (int(m.group(1)), int(m.group(2)))

    rel = report.relative_to(ROOT)
    if title is None:
        raise ParseError(f"{rel}: no '# <NAME> — validation report' heading")
    if findings is None:
        raise ParseError(f"{rel}: no '- **N findings**, M of them ...' line")
    if limits is None:
        raise ParseError(f"{rel}: no '- **N/M limits** hold' line")

    # .../<module>/tests/validation/<object>/results.md
    obj = report.parent.name
    # `<module>/tests/validation/<obj>/` for a bound object; for one with no
    # binding the folder is `tests/validation/<obj>/` and there is no module
    # to name, which the table says rather than inventing one.
    module = report.parents[3].name
    if module == "doppler":
        module = "— (C only)"
    return {
        "object": obj,
        "module": module,
        "title": title,
        "findings": findings[0],
        "open": findings[1],
        "open_ids": findings[2] or "",
        "limits_pass": limits[0],
        "limits_total": limits[1],
        "report": rel.as_posix(),
    }


def render(rows: list[dict[str, object]]) -> list[str]:
    """The generated block: a coverage line, then one row per object.

    The coverage line reports two populations rather than one ratio. It
    used to read "N of 71 objects certified" against a denominator of
    `objects/*.toml`, which the numerator is not drawn from: a validation
    folder is named for the C core, and three certified surfaces —
    `ema`, `mpsk`, `resamp` — have no object manifest at all, being
    function primitives or cores declared another way. Dividing one
    population by the other overstated coverage, and at the limit could
    have printed a numerator larger than its denominator.
    """
    manifests = {p.stem for p in OBJECTS.glob("*.toml")}
    named = sorted(str(r["object"]) for r in rows)
    with_manifest = [o for o in named if o in manifests]
    without = [o for o in named if o not in manifests]
    line = (
        f"**{len(rows)} objects certified** — {len(with_manifest)} of the "
        f"{len(manifests)} `objects/*.toml` jm fragments"
    )
    if without:
        line += (
            f", plus {len(without)} with no object manifest at all "
            f"({', '.join(f'`{o}`' for o in without)}): a function "
            f"primitive, or a core declared another way. "
        )
    else:
        line += ". "
    line += (
        "Not every fragment is a DSP object with an envelope worth "
        "certifying, so read the denominator as a ceiling rather than a "
        "target — and note the two counts are different populations, not "
        "a percentage."
    )
    out = [START_MARKER, "", line, ""]
    header = ["object", "module", "limits", "findings", "still open"]
    out.append("| " + " | ".join(header) + " |")
    out.append("|" + "|".join("---" for _ in header) + "|")
    for r in rows:
        limits = f"{r['limits_pass']}/{r['limits_total']}"
        # A failing limit is a regression and the thing a reader scans
        # for, so it is marked rather than left to arithmetic. An open
        # finding is NOT marked that way: every certified object here has
        # all its limits holding while carrying open findings, so bolding
        # them would read as failure when it means "documented, not
        # closed". The IDs are given instead, so the row points at them.
        if r["limits_pass"] != r["limits_total"]:
            limits = f"**{limits}**"
        open_cell = f"{r['open']} — {r['open_ids']}" if r["open"] else "none"
        out.append(
            f"| [{r['title']}]({BLOB}/{r['report']}) "
            f"| `{r['module']}` | {limits} | {r['findings']} "
            f"| {open_cell} |"
        )
    out += ["", END_MARKER]
    return out


def main() -> int:
    if "--write" not in sys.argv and "--check" not in sys.argv:
        print("usage: gen_validation_log.py [--write|--check]")
        return 2
    check_only = "--check" in sys.argv

    reports = sorted({p for g in GLOBS for p in SRC.glob(g)})
    try:
        rows = [parse(r) for r in reports]
    except ParseError as exc:
        print(f"gen_validation_log: FAIL — {exc}", file=sys.stderr)
        print(
            "  The summary block is written by Report.summary() in "
            "src/doppler/tests/_validation_common.py.\n"
            "  If its format changed, change this generator with it.",
            file=sys.stderr,
        )
        return 1

    rows.sort(key=lambda r: (str(r["module"]), str(r["object"])))
    block = "\n".join(render(rows))

    text = PAGE.read_text(encoding="utf-8")
    pattern = re.compile(
        re.escape(START_MARKER) + r".*?" + re.escape(END_MARKER),
        re.S,
    )
    if not pattern.search(text):
        print(
            f"gen_validation_log: FAIL — {PAGE.relative_to(ROOT)} has no "
            f"{START_MARKER} / {END_MARKER} pair",
            file=sys.stderr,
        )
        return 1
    updated = pattern.sub(lambda _: block, text)
    # Content, not layout: mdformat owns the padding (see normalize).
    # Both sides go through it, so an already-formatted page is neither
    # reported stale nor rewritten back to the unpadded form.
    drifted = normalize(updated) != normalize(text)

    if check_only:
        if drifted:
            print(
                "gen_validation_log: STALE — "
                f"{PAGE.relative_to(ROOT)} does not match the reports.\n"
                "  Run `make docs-relink`.",
                file=sys.stderr,
            )
            return 1
        print(
            f"gen_validation_log: OK — {len(rows)} certified object(s) "
            f"in {PAGE.relative_to(ROOT)}"
        )
        return 0

    if drifted:
        PAGE.write_text(updated, encoding="utf-8")
        print(f"gen_validation_log: rewrote {PAGE.relative_to(ROOT)}")
    print(f"gen_validation_log: {len(rows)} certified object(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
