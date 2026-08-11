"""Shared machinery for the per-object validation reports.

Each object certified under the validation campaign owns a folder beside
its module's tests — `src/doppler/<module>/tests/validation/<object>/` —
holding a `validate.py`, the `results.md` it generates, the plots that
report embeds and the raw sweeps under `data/`. This module is what they
all stand on, so the report format cannot drift between objects and the
`Report` class is not copied 71 times.

The split that matters is between **measuring** and **writing**. A
report's three phases (characterise, review, limits) are pure
measurement; emitting `results.md`, the PNGs and the CSVs is a side
effect. Keeping them separable is what lets the limits run as ordinary
pytest cases — see `build()` below and each module's
`test_validation_limits.py`.

That separation exists because the alternative was measured and failed:
for its first two objects the validation tree was run by nothing at all —
no make target, no CI job, and `pytest --collect-only` found zero tests
in it — so a section headed "Claims a caller may rely on. A failure here
is a regression" was asserted by nobody. Two regressions duly slipped
through it in one afternoon and were caught by hand.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterator
    from pathlib import Path


#: Below this, an EVM is not a measurement -- it is "essentially zero".
#:
#: A noiseless stream through a filter matched to it leaves an error vector
#: at the float floor, and `ber_evm_db` duly reports something like -321 dB.
#: That number is the machine's epsilon talking, not the object's, and it
#: does not survive a change of compiler, platform or summation order. Left
#: in a report it reads as a measurement of extraordinary quality, and any
#: limit written against it would be pinned to digits that mean nothing.
#:
#: The floor is set by the DATA PATH, not by a margin over today's numbers.
#: doppler measures on `complex64`, so the smallest non-zero error the
#: harness can represent is one float32 ULP -- and that is a perfectly good
#: measurement. Measured, injecting a known error into `ber_evm_db`:
#:
#:      0 ULP  ->  -321.3 dB      exact cancellation: not a measurement
#:      1 ULP  ->  -138.5 dB      the arithmetic floor: still real
#:      8 ULP  ->  -120.4 dB
#:   1024 ULP  ->   -78.3 dB
#:
#: So everything from about -138 dB upward is genuine, resolvable error,
#: and the only value BELOW that floor is exactly zero. -150 is the first
#: round number under it: it clamps exact cancellation and nothing else.
#:
#: Two earlier candidates were rejected by that table. **-50** and **-80**
#: both sit inside the resolvable range and would silently destroy real
#: figures -- this constant's own failure mode, arriving from the other
#: side. (-80 looked safe against the deepest number then on record, -47.5
#: dB at `bn = 0.002, sps = 4`; the deepest number ON RECORD is not the
#: deepest number POSSIBLE, which is what the ULP floor gives.) **-300**
#: fails the opposite way: the exact-cancellation case reports -321.3, so a
#: -300 floor clamps it to another absurd-looking figure and separates
#: nothing.
#:
#: These reports measure controlled, noiseless, single-impairment cases
#: deliberately -- that isolation is what makes a property attributable --
#: so they reach depths a deployed link never would, and the floor has to
#: respect that rather than link-realistic intuition.
#:
#: This is the mirror of the sentinel trap F4 records at the other end,
#: where `ber_evm_db`'s 0.0 dB "no lock" answer read as data in a table.
#: Both ends of the scale need saying in words rather than digits.
EVM_FLOOR_DB = -150.0


def clamp_evm_db(evm: float) -> float:
    """Floor an EVM at @ref EVM_FLOOR_DB, where it stops being a number.

    Parameters
    ----------
    evm : float
        EVM in dB, as `ber_evm_db` reports it.

    Returns
    -------
    float
        `evm`, or `EVM_FLOOR_DB` when it is below the floor.
    """
    return max(float(evm), EVM_FLOOR_DB)


@dataclass
class Report:
    """A validation report under construction: markdown, findings, limits.

    Attributes
    ----------
    lines : list of str
        The markdown emitted so far, one entry per line.
    findings : list of (tag, verdict, text)
        Phase 2's judgements. `verdict` is one of BY DESIGN, GAP,
        CONFIRMED, FIXED or C-ONLY; the first two counts in the summary
        treat GAP and CONFIRMED as open.
    limits : list of (ok, claim)
        Phase 3's envelope. Every one of these is asserted by a pytest
        case, so a False here fails CI rather than only reading badly.
    write : bool
        False when the report is being built purely to check its limits,
        which suppresses every file the run would otherwise emit.
    """

    lines: list[str] = field(default_factory=list)
    findings: list[tuple[str, str, str]] = field(default_factory=list)
    limits: list[tuple[bool, str]] = field(default_factory=list)
    write: bool = True

    # ── markdown ─────────────────────────────────────────────────────
    def md(self, text: str = "") -> None:
        """Append one line of markdown."""
        self.lines.append(text)

    def table(self, header: list[str], rows: list[list[str]]) -> None:
        """Append a pipe table, followed by a blank separator line."""
        self.md("| " + " | ".join(header) + " |")
        self.md("|" + "|".join("---" for _ in header) + "|")
        for r in rows:
            self.md("| " + " | ".join(str(c) for c in r) + " |")
        self.md()

    # ── phases 2 and 3 ───────────────────────────────────────────────
    def find(self, tag: str, verdict: str, text: str) -> None:
        """Record one review finding and echo it to the console."""
        self.findings.append((tag, verdict, text))
        print(f"  [{verdict:^10}] {tag}: {text[:96]}")

    def limit(self, ok: bool, claim: str) -> bool:
        """Record one envelope claim and whether it holds."""
        self.limits.append((bool(ok), claim))
        print(f"  [{'PASS' if ok else 'FAIL':^10}] {claim[:96]}")
        return bool(ok)

    # ── output ───────────────────────────────────────────────────────
    @property
    def open_findings(self) -> list[tuple[str, str, str]]:
        """Findings still counted against the object."""
        return [f for f in self.findings if f[1] in ("GAP", "CONFIRMED")]

    def failures(self) -> Iterator[str]:
        """Every limit that does not hold, as its claim text."""
        return (claim for ok, claim in self.limits if not ok)

    def summary(self, extra: str = "") -> None:
        """Append the closing summary section."""
        gaps = self.open_findings
        # No trailing space when the list is empty: the end-of-line hook
        # would strip it and the next regeneration would put it back,
        # which is a generator-versus-formatter loop.
        tail = f": {', '.join(g[0] for g in gaps)}" if gaps else " — none left"
        npass = sum(1 for ok, _ in self.limits if ok)
        self.md()
        self.md("## 5. Summary")
        self.md()
        self.md(
            f"- **{len(self.findings)} findings**, {len(gaps)} of them gaps "
            f"or confirmed defects{tail}\n"
            f"- **{npass}/{len(self.limits)} limits** hold" + extra
        )
        self.md()

    def _self_check(self, text: str) -> list[str]:
        """Internal inconsistencies a staleness gate cannot see.

        `make validate-check` proves `results.md` matches what
        `validate.py` renders. It cannot say whether what is rendered is
        COHERENT — so a report can point at a section that does not
        exist, skip a number, or ship a sentence truncated mid-reference,
        and stay green forever because the generator reproduces it
        faithfully every time. All three shipped before this existed.

        Checked here rather than in a separate script so it is
        registration-free: every object's `build()` renders through this
        method, so a new object is covered the moment it exists, and
        `make validate`, `make validate-check` and the per-module limits
        test all enforce it without any of them naming it.
        """
        problems: list[str] = []
        heads = set(re.findall(r"^#{2,3} (\d+(?:\.\d+)?)\s", text, re.M))

        for ref in sorted(set(re.findall(r"§(\d+\.\d+)", text))):
            if ref not in heads:
                problems.append(
                    f"§{ref} is referenced but no such section exists "
                    f"(have: {', '.join(sorted(heads))})"
                )

        subs = sorted(
            (h for h in heads if h.startswith("2.")),
            key=lambda h: int(h.split(".")[1]),
        )
        nums = [int(h.split(".")[1]) for h in subs]
        if nums and nums != list(range(1, len(nums) + 1)):
            problems.append(
                f"section 2 numbering is not sequential: {', '.join(subs)}"
            )

        # `txt.split(".")[0]` truncation cuts at the first period, so a
        # sentence containing a decimal ends mid-reference.
        for cell in re.findall(r"\|[^|\n]*?\(§?\d+\.\s*\|", text):
            problems.append(
                f"table cell truncated mid-reference: {cell.strip()[:70]}"
            )
        return problems

    def render(self) -> str:
        """The report as one string, ending in exactly one newline.

        The rstrip is load-bearing: `md()` with no argument emits a blank
        separator, and a trailing one would leave two newlines, which the
        end-of-file-fixer rewrites — putting the generator and the
        formatter in a loop.

        Raises
        ------
        ValueError
            If the report contradicts itself — see `_self_check`.
        """
        text = "\n".join(self.lines).rstrip("\n") + "\n"
        problems = self._self_check(text)
        if problems:
            raise ValueError(
                "the report is internally inconsistent:\n  - "
                + "\n  - ".join(problems)
            )
        return text

    def emit(self, path: Path) -> None:
        """Write the report, unless this run is measurement-only."""
        if self.write:
            path.write_text(self.render())


def cli(build, here: Path) -> int:
    """Run one object's `build()` as a script, or as a staleness check.

    Parameters
    ----------
    build : callable
        `build(write: bool) -> Report` — the object's own runner.
    here : Path
        The object's validation folder, holding `results.md`.

    Returns
    -------
    int
        Process exit status, answering ONE question per mode. Under
        `--check` it is staleness alone: 0 when `results.md` matches a
        fresh run, 1 when it does not. Without `--check` it is the limits:
        0 when they all hold, 1 when any fails.

    Notes
    -----
    `--check` is what stops the committed report drifting from the code
    that generates it, the same way the generated doc regions are
    guarded. It renders in memory and compares; it never writes.

    **`--check` deliberately does NOT fail on a failing limit**, and the
    separation is the point. `make validate-check` reads only this exit
    status, so folding the limits in made it report "STALE — re-run
    `make validate`" for a report that was perfectly up to date, sending
    the reader to a command that changes nothing. Measured: after the
    resamp ctrl fix improved RateSync, `--check` printed "up to date" and
    then exited 1 anyway, because 24/26 limits held.

    The limits already have a gate that answers only them --
    `src/doppler/<module>/tests/test_validation_limits.py`, which runs
    each object's own `build(write=False)` in the ordinary pytest suite --
    so nothing is unguarded by this. Two gates, two questions, two
    diagnoses a reader can act on.
    """
    import sys

    check = "--check" in sys.argv[1:]
    report = build(write=not check)
    out = here / "results.md"

    if check:
        current = out.read_text() if out.exists() else ""
        if current != report.render():
            print(f"  STALE: {out} differs from a fresh run — re-run it")
            return 1
        print(f"  up to date: {out.relative_to(here.parents[5])}")

    failed = list(report.failures())
    npass = len(report.limits) - len(failed)
    gaps = report.open_findings
    print(f"\n{'━' * 70}")
    print(
        f"  {len(report.findings)} findings ({len(gaps)} gaps/defects), "
        f"{npass}/{len(report.limits)} limits held"
    )
    if not check:
        print(f"  wrote {out}")
    if check and failed:
        print(
            f"  ({len(failed)} limit(s) failing — that is "
            f"test_validation_limits.py's verdict, not this gate's)"
        )
    print(f"{'━' * 70}")
    if check:
        return 0  # staleness was decided above; limits are pytest's
    return 1 if failed else 0
