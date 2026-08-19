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

A fourth step composes rather than measures: `Report.executive` writes
the front matter — the status line and the key takeaways — and it runs
LAST because its status counts limits and findings that do not exist
until every phase has. It renders FIRST. That ordering lives in
`render()` rather than in call order, which is why the summary is
accumulated in `head` and not appended to `lines`.

That separation exists because the alternative was measured and failed:
for its first two objects the validation tree was run by nothing at all —
no make target, no CI job, and `pytest --collect-only` found zero tests
in it — so a section headed "Claims a caller may rely on. A failure here
is a regression" was asserted by nobody. Two regressions duly slipped
through it in one afternoon and were caught by hand.
"""

from __future__ import annotations

import difflib
import re
from contextlib import contextmanager
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from collections.abc import Iterator


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

#: The review phase's whole vocabulary, and which verdicts count as OPEN.
#:
#: One home, because it had three and they disagreed in the direction that
#: hides work: the five names lived in a docstring, the open pair lived as
#: a literal tuple inside `open_findings`, and `find()` validated neither.
#: A verdict is just a string on the way in, so `"Gap"`, `"GAP "` or an
#: invented `"OPEN"` was accepted and then silently counted as NOT open --
#: a typo downgrades a real defect to invisible, in the executive summary
#: and in the validation log's `still open` column at once.
#:
#: A verdict is a judgement about a PROBLEM. There is deliberately none
#: meaning "this works": that is what a limit is for, and a passing result
#: recorded as a finding inflates the count with something already gated.
#: `CONFIRMED` is the one that reads backwards -- it means a confirmed
#: DEFECT, not a confirmed claim. mpsk shipped two positive results under
#: it for one commit and advertised three open findings against one real
#: one; see `docs/dev/contributing/validation.md`.
VERDICTS = ("BY DESIGN", "GAP", "CONFIRMED", "FIXED", "C-ONLY")

#: The subset that counts against the object. Derived from nothing else --
#: every consumer reads it here.
OPEN_VERDICTS = ("GAP", "CONFIRMED")

#: Spans `_structural` must NOT mask, because they are structure written with
#: digits. A section heading's number IS the section numbering, a `§N.M` is a
#: cross-reference to it, and `#N` / `gh-N` / `issues/N` is the issue an open
#: finding is required to cite -- masking any of them would let a renumbered
#: section or a re-pointed citation through the gate silently.
_PROTECTED = (
    re.compile(r"§\d+(?:\.\d+)?"),
    re.compile(r"gh-\d+"),
    re.compile(r"#\d+"),
    re.compile(r"issues/\d+"),
)

#: A numeric literal, as a report prints one: `8`, `-0.35`, `.5`, `1.29e-02`.
_NUMBER = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")

#: What a masked number becomes. Any fixed string works; this one cannot occur
#: in a report, so a diff of masked text is unambiguous.
_MASK = "«n»"

#: Stand-in for a protected span while numbers are masked around it.
#: DIGIT-FREE, deliberately: an index-numbered placeholder gets masked by
#: `_NUMBER` itself, which silently destroys every protected span.
_HOLD = "\x01"

#: Diff lines `--check` prints before truncating.
#:
#: Enough to characterise the difference -- a handful of changed numbers
#: reads as a machine difference, a changed section reads as a real edit --
#: without a wall of output when a generator changed shape. `n=0` context,
#: so every line shown is a changed one.
_DIFF_LINES = 40


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


def _structural(text: str) -> str:
    """`text` with measured numbers masked, so structure can be compared.

    This is what `--check` actually gates, and the split is the whole
    design. **A validation report's numbers are measurements and its
    structure is code**, so they need different contracts:

    - the STRUCTURE -- which sections exist and in what order, the prose,
      each limit's claim wording, every verdict and finding tag, the shape
      of every table -- is determined by `validate.py`, so a difference
      means the committed report no longer matches the code. That is
      staleness, and it is byte-exact here.
    - the NUMBERS are the output of stochastic measurements over finite
      records, so they carry their own uncertainty and differ between
      toolchains without anything being wrong.

    Why numbers are not compared with a tolerance instead, which is the
    obvious idea and was the plan until it was measured. The observed
    toolchain differences, gcc 15.2/glibc 2.43 against gcc 13.3/glibc 2.39
    on one CPU:

        mpsk BPSK SER    1.332e-02 -> 1.292e-02      3.0%
        mpsk errors            204 -> 198            2.9%
        mpsk QPSK loss        0.39 -> 0.35 dB       10.3%
        ratesync spread        0.3 -> 0.2 dB        33.3%
        ratesync rate            5 -> 6 ppm         16.7%
        carrier_nda freq  -3.45e-8 -> -1.22e-9      96.5%

    A single relative tolerance would have to exceed **96%** to accept all
    of them, which gates nothing. The reason is structural, not a bad
    choice of number: relative deviation grows without bound as a quantity
    approaches zero, and these reports deliberately measure quantities that
    converge to zero. Per-quantity tolerances would work, but the compared
    artifact is markdown -- at this point a `0.3` in a table has no units
    and no provenance, so there is nothing to key a tolerance on.

    So the numbers are gated where they have both: each object's
    `test_validation_limits.py`, which asserts them through the same
    `build()` with thresholds the report's author chose per quantity, in
    the units they belong to. `--check` answers "does the committed report
    still describe this code", and the limits gate answers "are the numbers
    still inside the envelope". Two gates, two questions -- the same split
    `cli()` already documents for the limits themselves.

    What this deliberately does NOT catch: a behaviour change that moves a
    number without moving any structure and without breaching any limit.
    That is not a gap -- a number that moved inside the certified envelope
    is the definition of "not a regression".

    Parameters
    ----------
    text : str
        A rendered report.

    Returns
    -------
    str
        `text` with every numeric literal replaced by `_MASK`, except in
        headings and in the protected reference forms.
    """
    out = []
    for line in text.split("\n"):
        if re.match(r"^#{1,6} ", line):
            # A heading's digits ARE the section numbering. Keep them.
            out.append(line)
            continue
        held: list[str] = []

        def _hold(m: re.Match[str], _h: list[str] = held) -> str:
            _h.append(m.group(0))
            # The sentinel must contain NO DIGITS. An index-numbered
            # placeholder was tried and `_NUMBER` promptly masked the index,
            # so every protected span was destroyed and the two misses this
            # caught -- a changed `§2.5` and a re-pointed `gh-733` -- sailed
            # through the gate. Order is preserved instead, and restored by
            # consuming the list in the same order.
            return _HOLD

        for pat in _PROTECTED:
            line = pat.sub(_hold, line)
        line = _NUMBER.sub(_MASK, line)
        if held:
            it = iter(held)
            line = re.sub(_HOLD, lambda _m, _i=it: next(_i), line)
        out.append(line)
    return "\n".join(out)


def _numeric_drift(committed: str, fresh: str) -> tuple[int, float]:
    """How many masked numbers differ, and the worst relative deviation.

    Reported but never gated -- see `_structural`. It is here so a
    structurally-identical report that drifted numerically says so out
    loud: silence would leave a reader unable to tell "this machine agrees"
    from "this machine was never compared".

    Returns
    -------
    (int, float)
        Count of differing numbers, and the largest relative difference
        (1.0 when a value moved to or from zero). `(0, 0.0)` when the two
        texts carry a different COUNT of numbers, which `_structural`
        catches on its own and reports better.
    """
    a = _NUMBER.findall(committed)
    b = _NUMBER.findall(fresh)
    if len(a) != len(b):
        return 0, 0.0
    n, worst = 0, 0.0
    for x, y in zip(a, b):
        if x == y:
            continue
        n += 1
        fx, fy = float(x), float(y)
        scale = max(abs(fx), abs(fy))
        worst = max(worst, abs(fx - fy) / scale if scale else 0.0)
    return n, worst


def _cell(value: object) -> str:
    """Render one table cell so its content cannot become structure.

    A pipe inside a cell is a column delimiter unless escaped, and a DSP
    report reaches for one constantly -- ``|gain_db|``, ``|per-sample -
    chunked|``, ``|H(f)|``. Escaping here rather than at each call site is
    the point: a report author writing a magnitude should not have to know
    the table is pipe-delimited.
    """
    return str(value).replace("|", r"\|")


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
    head : list of str
        Front matter — the executive summary. Written LAST, because its
        status line counts limits and findings that do not exist until
        every phase has run, and rendered FIRST. Kept as its own list
        rather than inserted into `lines` so the ordering is a property of
        `render()` and not of who called what when.
    """

    lines: list[str] = field(default_factory=list)
    findings: list[tuple[str, str, str]] = field(default_factory=list)
    limits: list[tuple[bool, str]] = field(default_factory=list)
    write: bool = True
    head: list[str] = field(default_factory=list)

    # ── markdown ─────────────────────────────────────────────────────
    def md(self, text: str = "") -> None:
        """Append one line of markdown."""
        self.lines.append(text)

    def table(self, header: list[str], rows: list[list[str]]) -> None:
        """Append a pipe table, followed by a blank separator line.

        Every cell is escaped, because a DSP report writes absolute values
        constantly and ``|gain_db|`` is a column delimiter to markdown, not
        a magnitude. Two shipped reports were malformed this way -- the AGC's
        ``worst |gain_db| over 4000 on-target samples`` header parsed as four
        columns against a two-column body, and the EMA's
        ``|per-sample - chunked|`` as five against three.

        Neither was caught, and ``make validate-check`` structurally cannot
        catch it: that gate re-runs the generator and compares, so a
        generator emitting broken markdown agrees with itself perfectly.
        ``scripts/check_validation_tables.py`` is the gate that reads the
        rendered table instead.
        """
        self.md("| " + " | ".join(_cell(c) for c in header) + " |")
        self.md("|" + "|".join("---" for _ in header) + "|")
        for r in rows:
            self.md("| " + " | ".join(_cell(c) for c in r) + " |")
        self.md()

    # ── phases 2 and 3 ───────────────────────────────────────────────
    def find(self, tag: str, verdict: str, text: str) -> None:
        """Record one review finding, render it, and echo it.

        All three from one call, deliberately. Recording and rendering used
        to be separate, and only recording was ever wired up: every report
        headed section 3 "Findings, with verdicts" and then listed **none**
        of them. Six reports, every finding, invisible -- they existed only
        in this function's console output, which nothing keeps.

        The reports cross-reference into that void: the EMA's claim table
        says "recorded as §3 F6" and the AGC's says "see §3 F4", pointing
        at a section with no content. Phase 2 of
        ``docs/dev/contributing/validation.md`` is the review, and the artifact
        that is
        supposed to carry it carried a heading.

        Every ``find()`` is called from a validator's ``review()``, right
        after the section 3 heading, so appending here lands in the right
        place with no ordering rule for authors to remember.
        """
        self.findings.append((tag, verdict, text))
        self.md(f"- **{tag} · {verdict}** — {text}")
        self.md()
        print(f"  [{verdict:^10}] {tag}: {text[:96]}")

    def limit(self, ok: bool, claim: str) -> bool:
        """Record one envelope claim and whether it holds."""
        self.limits.append((bool(ok), claim))
        print(f"  [{'PASS' if ok else 'FAIL':^10}] {claim[:96]}")
        return bool(ok)

    # ── front matter ─────────────────────────────────────────────────
    def executive(
        self, title: str, takeaways: list[str], *, source: str | None = None
    ) -> None:
        """Render the executive summary: status, then what a reader needs.

        Called last and read first. Everything in the status line is
        DERIVED from what the run recorded — the limit tally, the finding
        tally, which findings are still open — so it cannot drift from the
        body the way a hand-written abstract does. The `takeaways` are the
        one authored part, because "what matters here" is judgement and no
        counter produces it.

        Parameters
        ----------
        title : str
            The object, as a caller names it: ``"CarrierNda"``.
        takeaways : list of str
            Short, complete sentences. Each should be something a caller
            would change a decision over — a number to design to, a
            failure mode to defend against, a limit of the evidence. Three
            to six; a list of twelve is the body of the report again.

        Notes
        -----
        **Nothing here may be time-varying.** `make validate-check`
        re-renders the report and compares bytes, so a generated date, a
        duration or a hostname would make every report permanently stale
        and send the reader to a command that changes nothing. The status
        is a function of the measurements alone.
        """
        npass = sum(1 for ok, _ in self.limits if ok)
        total = len(self.limits)
        gaps = self.open_findings
        if not total:
            status = "**NOT CERTIFIED** — this report asserts no limits"
        elif npass < total:
            failed = total - npass
            status = (
                f"**REGRESSED** — {failed} of {total} limits "
                f"{'does' if failed == 1 else 'do'} not hold, and a failure "
                f"there is a regression rather than a new finding"
            )
        else:
            status = (
                "**CERTIFIED** — the single limit holds"
                if total == 1
                else f"**CERTIFIED** — all {total} limits hold"
            )

        # Says WHICH are open and stops there. An earlier draft added "each
        # filed as an issue", which is a claim about the tracker that this
        # code cannot see -- true for the object it was written against and
        # unverified for every other. Whether a finding is filed belongs in
        # that finding's own text, where it can be checked.
        tail = (
            f"{len(gaps)} still open: {', '.join(g[0] for g in gaps)}"
            if gaps
            else "none open"
        )
        self.head.append(f"# {title} — validation report")
        self.head.append("")
        # The provenance sentence is a CLAIM, so a component with no binding
        # must not ship the default one. `conv` is measured by a C harness
        #  this file renders (docs/dev/contributing/validation.md, "certifying
        # a component
        # with no binding"); saying "through its own binding" there would be
        # the report's first sentence being false.
        self.head.append(
            source
            or (
                "Generated by `validate.py` in this folder. Every number is "
                "measured from the C implementation through its own "
                "binding; nothing is modelled. Re-run to regenerate."
            )
        )
        self.head.append("")
        self.head.append("## Executive summary")
        self.head.append("")
        self.head.append(f"- **Status.** {status}.")
        self.head.append(
            f"- **Findings.** {len(self.findings)} recorded, {tail}."
        )
        self.head.append("")
        self.head.append("**What a caller most needs to know**")
        self.head.append("")
        for t in takeaways:
            self.head.append(f"- {t}")
        self.head.append("")

    # ── output ───────────────────────────────────────────────────────
    @property
    def open_findings(self) -> list[tuple[str, str, str]]:
        """Findings still counted against the object."""
        return [f for f in self.findings if f[1] in OPEN_VERDICTS]

    def failures(self) -> Iterator[str]:
        """Every limit that does not hold, as its claim text."""
        return (claim for ok, claim in self.limits if not ok)

    def summary(self, extra: str = "") -> None:
        """Close section 4 with the limits table, then append section 5.

        **The table is emitted here rather than by each object's own
        `limits()`, and that is the whole point.** Section 4 exists to
        state the envelope a caller may rely on, and four of the eleven
        certified objects rendered it while seven rendered a heading, the
        sentence "Claims a caller may rely on", and then nothing at all --
        agc, ema, resamp, lockdet, mpsk, loop_filter and mpsk_receiver.
        Every one of them closed with `**33/33 limits** hold` in section
        5, so the count was right there beside a section that named none
        of them.

        Neither gate could see it. `test_validation_limits.py` asserts the
        limits and never reads the report; `make validate-check`
        re-renders and compares bytes, so a generator that emits an empty
        section agrees with itself perfectly. It is the same shape as the
        malformed tables `table()` documents, and it takes the same fix:
        move it into the one place every report goes through.

        `summary()` is that place -- every validator calls it, and it is
        the only hook that runs after the last `limit()` -- so the table
        cannot be forgotten by a new object and the four hand-rolled
        copies are gone rather than left to drift. `_self_check` pins the
        outcome: a rendered report must carry one row per recorded limit.
        """
        if self.limits:
            self.md()
            self.table(
                ["verdict", "claim"],
                [
                    ["PASS" if ok else "**FAIL**", claim]
                    for ok, claim in self.limits
                ],
            )
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

        # A verdict outside the vocabulary is not a typo the reader can
        # see: `open_findings` matches exact strings, so anything else is
        # counted as CLOSED. It fails in the direction that hides work.
        for tag, verdict, _ in self.findings:
            if verdict not in VERDICTS:
                problems.append(
                    f"{tag} has verdict {verdict!r}, which is not one of "
                    f"{', '.join(VERDICTS)} — an unknown verdict is "
                    f"silently counted as not open"
                )

        # An open finding is a defect the object still carries, and the
        # repo's rule is that a carve-out gets FILED rather than explained
        # in prose. Enforced here because a gap recorded only inside a
        # report is invisible to everyone not reading that report -- agc's
        # F6 (59.9 dB applied to a noise floor) sat that way until gh-750.
        #
        # It is also what catches a POSITIVE result mislabelled as open:
        # "the header's figure is confirmed correct" has no issue to cite,
        # because there is nothing to fix. That is the mpsk mistake, and
        # this is the check that would have caught it.
        for tag, verdict, body in self.findings:
            if verdict in OPEN_VERDICTS and not re.search(
                r"gh-\d+|#\d+|issues/\d+", body
            ):
                problems.append(
                    f"{tag} is {verdict} — an open finding must cite the "
                    f"issue tracking it (gh-N or #N). If there is nothing "
                    f"to file, it is not open: a result that holds is a "
                    f"limit, not a finding"
                )

        # Section 4 must STATE the envelope, not just count it. Seven of
        # eleven reports rendered the heading and no rows while section 5
        # closed with "N/N limits hold", so the only place a caller could
        # read what was certified was the source of `validate.py`. Counted
        # against the rendered text rather than trusting `summary()` to
        # have run, because that is the artifact a reader gets.
        pat = r"^\| (?:PASS|\*\*FAIL\*\*) \| "
        rendered = len(re.findall(pat, text, re.M))
        if len(self.limits) != rendered:
            problems.append(
                f"section 4 renders {rendered} limit rows against "
                f"{len(self.limits)} recorded — the envelope a caller may "
                f"rely on has to be IN the report, not only counted by it"
            )

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
        text = "\n".join(self.head + self.lines).rstrip("\n") + "\n"
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

    @contextmanager
    def capture(
        self, data_dir: Path, name: str, clock=None, ring_records=1 << 22
    ):
        """Attach telemetry, capture the run, and FILE it as evidence.

        A report hands a reader settled numbers — an EVM, a lock time, an
        error rate — and the trajectories that produced them are not in it.
        This is where they land: the capture goes into the object's own
        `data/` folder beside the CSVs, committed like every other artifact
        there, so the scene behind a number is re-openable without re-running
        anything (doppler#846).

        **`Report` owns this because it already owns the folder.** It is the
        single writer of `results.md`, the plots and `data/*.csv`; a
        per-validator convention would let the layout drift between objects
        the way the report format would without this module.

        The ORDER is the part worth encapsulating, and it is the same reason
        `dp_ber_measure()` exists on the C side: probes must be registered
        BEFORE the capture opens, because the ring is sized from the probe
        table and a capture opened first has no bound to size to. So the
        sequence is attach, then `arm`, then run — and getting it wrong is a
        `ValueError` from the library rather than a silent truncation.

        Completeness is not this method's invention either: `close()` raises
        if the ring lost a record, and that is what makes a filed capture
        evidence rather than a recording.

        **Be precise about where the guarantee comes from, though.** The
        library's own is that a ring sized to `probe_count * block_samples`
        and drained at every boundary CANNOT overflow — but neither factor is
        known until the probes are attached and `arm` is called, and the
        context has to exist before either. So the ring is allocated
        generously (`ring_records`, defaulting to the size the receiver
        harness has run on for a year) and `close()`'s refusal is the
        BACKSTOP, not the mechanism. Measured: a ring genuinely under the
        bound raises *"the capture has a hole: records were dropped, which
        the block bound makes impossible unless..."*, so an under-allocation
        is loud rather than a short file. Raise `ring_records` if it fires.

        `write=False` still captures — every measurement runs, exactly as the
        limits gate expects — but files nothing, because a test must never
        write into the repo.

        Parameters
        ----------
        data_dir : Path
            The object's `data/` folder. Created if absent.
        name : str
            Basename for the capture; `<name>.tlm` and its `-meta` sidecar.
        clock : Any, optional
            The pipeline's sample clock, borrowed for the sidecar's time
            base. None states there is no time base.
        ring_records : int, optional
            Ring capacity, a power of two. The default is what the M-PSK
            receiver harness uses; a capture that outgrows it refuses rather
            than truncating.

        Yields
        ------
        _Capture
            Carries `.telemetry` to attach, `.arm(block_samples)` to open,
            and — after the block exits — `.probes`, the per-probe series by
            name, and `.path`, the filed capture or None.

        Examples
        --------
        >>> from pathlib import Path
        >>> from doppler.tests._validation_common import Report
        >>> R = Report(write=False)
        >>> with R.capture(Path("data"), "demo") as cap:
        ...     pid = cap.telemetry.probe("demo.x")
        ...     cap.arm(64)
        ...     cap.telemetry.emit(pid, 1.0)
        >>> [float(v) for v in cap.probes["demo.x"]]
        [1.0]
        """
        import tempfile

        from doppler.telemetry import Capture, Telemetry

        cap = _Capture(Telemetry(ring_records))
        # A capture that is not going to be committed still gets WRITTEN,
        # to a temp file, so the measurement path is byte-identical between
        # `make validate` and the limits gate. Two code paths here would be
        # two things to keep in step, and the one nobody runs would rot.
        tmp = None
        if self.write:
            data_dir.mkdir(parents=True, exist_ok=True)
            target = data_dir / f"{name}.tlm"
        else:
            tmp = tempfile.TemporaryDirectory()
            target = Path(tmp.name) / f"{name}.tlm"
        try:
            cap._open = lambda blk: Capture(
                cap.telemetry, blk, str(target), clock
            )
            yield cap
            if cap._cap is None:
                raise ValueError(
                    f"capture {name!r}: arm() was never called, so nothing "
                    f"was captured — attach the probes, then arm(), then run"
                )
            cap.telemetry.set_now(cap._block)
            cap._cap.close()  # raises if the ring lost a record
            cap.probes = read_capture(target)
            cap.path = target if self.write else None
        finally:
            if tmp is not None:
                tmp.cleanup()


@dataclass
class _Capture:
    """Handle yielded by `Report.capture`. See that method."""

    telemetry: object
    probes: dict = field(default_factory=dict)
    path: object = None
    _cap: object = None
    _open: object = None
    _block: int = 0

    def arm(self, block_samples: int) -> None:
        """Open the capture, AFTER every probe is attached."""
        self._block = int(block_samples)
        self._cap = self._open(self._block)


def read_capture(path: Path) -> dict:
    """Per-probe series by name, read back from a filed capture.

    Reads the file rather than the in-memory records on purpose: it is the
    same path a plotting utility takes, so a capture that cannot be read back
    fails here — in the run that produced it — instead of later, in whatever
    reads it next.
    """
    import json

    import numpy as np

    dt = np.dtype(
        {
            "names": ["n", "value", "probe", "flags"],
            "formats": ["<u8", "<f4", "<u2", "<u2"],
            "offsets": [0, 8, 12, 14],
            "itemsize": 16,
        }
    )
    rec = np.fromfile(path, dtype=dt)
    meta = json.loads(Path(str(path) + "-meta").read_text())
    names = meta.get("probes", meta.get("probe_names", {}))
    if isinstance(names, list):
        names = {p["name"]: p.get("id", i) for i, p in enumerate(names)}
    return {
        n: rec[rec["probe"] == int(pid)]["value"].astype(float)
        for n, pid in names.items()
    }


def assert_renders(report: Report) -> None:
    """Assert one report is internally COHERENT, for the limits gate.

    `_self_check` runs from `render()`, and `render()` runs from `emit()`,
    which a `write=False` build skips -- so every coherence case (a `§N.M`
    with no such section, a gap in section 2's numbering, a cell truncated
    mid-reference, a limit counted but never rendered) reached the real
    reports only through `make validate` and `make validate-check`, and
    **neither is in any CI workflow**. `validate-check` sits in
    `GATES_DEPS` and no job runs `make gates`; grep the workflows for
    "validate" and the only hit is a release-time wheel check.

    So the checks were tested in CI -- `test_validation_report.py` drives
    them over seeded reports -- and never APPLIED in CI to the eleven
    reports they exist for. That is the campaign's founding bug one layer
    out: not a claim nobody executes, but a checker nobody points at the
    artifact.

    The limits gate is the right home because it already pays the cost:
    it builds every report to assert the envelope, so rendering one is
    pure string work on data already in memory. It closes coherence, not
    staleness -- whether the COMMITTED bytes match is still
    `make validate-check`'s question, and still not in CI (gh-816).

    Parameters
    ----------
    report : Report
        A built report, from `build(write=False)`.

    Raises
    ------
    AssertionError
        If the report contradicts itself, carrying `_self_check`'s
        complaint verbatim.
    """
    try:
        report.render()
    except ValueError as exc:
        raise AssertionError(str(exc)) from exc


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
        fresh = report.render()
        if _structural(current) != _structural(fresh):
            # SAY WHAT DIFFERS. "STALE" alone costs a round trip to answer
            # the first question anyone asks. Diffed on the MASKED text, so
            # what prints is the structural change itself rather than that
            # change buried in numeric noise -- masked numbers show as the
            # same token on both sides and drop out of the diff.
            diff = list(
                difflib.unified_diff(
                    _structural(current).splitlines(),
                    _structural(fresh).splitlines(),
                    fromfile=f"{out.name} (committed, numbers masked)",
                    tofile=f"{out.name} (fresh, numbers masked)",
                    lineterm="",
                    n=0,
                )
            )
            print(f"  STALE: {out} no longer matches its generator")
            shown = diff[:_DIFF_LINES]
            for line in shown:
                print(f"    {line}")
            if len(diff) > len(shown):
                print(f"    … {len(diff) - len(shown)} more diff line(s)")
            print("  Re-run 'make validate'.")
            return 1

        print(f"  up to date: {out.relative_to(repo_root(here))}")
        # Numeric drift is reported and NOT gated -- see `_structural`. Said
        # out loud because silence cannot distinguish "this machine agrees"
        # from "nothing compared the numbers".
        ndiff, worst = _numeric_drift(current, fresh)
        if ndiff:
            print(
                f"    (structure matches; {ndiff} number(s) differ, worst "
                f"{100 * worst:.1f}% — measurement noise, gated by "
                f"test_validation_limits.py rather than here)"
            )

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
