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

from dataclasses import dataclass, field
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterator
    from pathlib import Path


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

    def render(self) -> str:
        """The report as one string, ending in exactly one newline.

        The rstrip is load-bearing: `md()` with no argument emits a blank
        separator, and a trailing one would leave two newlines, which the
        end-of-file-fixer rewrites — putting the generator and the
        formatter in a loop.
        """
        return "\n".join(self.lines).rstrip("\n") + "\n"

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
        Process exit status: non-zero if any limit fails, or if `--check`
        finds `results.md` out of date with what `build()` produces.

    Notes
    -----
    `--check` is what stops the committed report drifting from the code
    that generates it, the same way the generated doc regions are
    guarded. It renders in memory and compares; it never writes.
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
    print(f"{'━' * 70}")
    return 1 if failed else 0
