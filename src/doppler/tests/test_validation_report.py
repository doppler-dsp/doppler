"""`Report._self_check` — the coherence gate, driven over seeded reports.

`make validate-check` proves a committed `results.md` matches what its
`validate.py` renders. It cannot say whether what is rendered is TRUE, so
a report can state something false and stay green forever because the
generator reproduces it faithfully every time. `_self_check` is the gate
for that class, and these are its cases.

Driven over reports built here rather than over the ten committed ones,
for the reason `issue-link-check`'s tests give: a gate proven only
against real content is proven only against the cases that happen to
exist today, and every one of them passes by construction.

Each case below corresponds to a mistake that actually shipped:

- a positive result filed under an open verdict (mpsk, one commit);
- an open finding with no issue behind it (agc F6, until gh-750, and
  RateSync F17 until gh-751);
- a `§x.y` pointing at a section the report does not have.

The verdict-vocabulary cases have no shipped instance, and are the reason
this file exists at all — `find()` took any string, `open_findings`
matched exact ones, so a typo counted a real defect as CLOSED. That fails
in the direction that hides work, and nothing would have printed a
warning.
"""

from __future__ import annotations

import pytest

from doppler.tests._validation_common import (
    OPEN_VERDICTS,
    VERDICTS,
    Report,
    _numeric_drift,
    _structural,
)


def _report(*findings: tuple[str, str, str]) -> Report:
    """A minimal renderable report carrying `findings`."""
    r = Report(write=False)
    r.md("# t")
    r.md()
    r.md("## 1. The object")
    r.md()
    for tag, verdict, body in findings:
        r.find(tag, verdict, body)
    return r


def _problems(r: Report) -> str:
    """Render, returning '' when coherent and the complaint when not."""
    try:
        r.render()
    except ValueError as exc:
        return str(exc)
    return ""


# ── the vocabulary is closed ─────────────────────────────────────────


@pytest.mark.parametrize("verdict", VERDICTS)
def test_every_documented_verdict_renders(verdict):
    """The five names in the vocabulary are all usable.

    Guards the gate against being wrong about compliant input, which is
    worse than no gate: an open verdict is given an issue so it satisfies
    the citation rule too.
    """
    body = "detail, gh-1" if verdict in OPEN_VERDICTS else "detail"
    assert _problems(_report(("F1", verdict, body))) == ""


@pytest.mark.parametrize("bogus", ["Gap", "gap", "GAP ", "OPEN", "PASS", ""])
def test_a_verdict_outside_the_vocabulary_is_rejected(bogus):
    """The failure mode is silent under-reporting, so it must be loud.

    `open_findings` matches exact strings. Before this check, each of
    these was accepted and then counted as NOT open — a real defect made
    invisible in the executive summary and in the validation log's
    `still open` column at once, by a typo.
    """
    out = _problems(_report(("F1", bogus, "real defect, gh-1")))
    assert "not one of" in out
    assert repr(bogus) in out


def test_open_verdicts_are_a_subset_of_the_vocabulary():
    """The two constants cannot drift apart."""
    assert set(OPEN_VERDICTS) <= set(VERDICTS)


# ── an open finding is filed, not merely written down ────────────────


@pytest.mark.parametrize("verdict", OPEN_VERDICTS)
@pytest.mark.parametrize("cite", ["gh-750", "#747", "issues/751"])
def test_an_open_finding_may_cite_an_issue_three_ways(verdict, cite):
    assert _problems(_report(("F1", verdict, f"broken — {cite}"))) == ""


@pytest.mark.parametrize("verdict", OPEN_VERDICTS)
def test_an_open_finding_with_no_issue_is_rejected(verdict):
    """agc F6's shape: a real gap that lived only inside one report.

    The repo's rule is that a carve-out gets filed rather than explained
    in prose, and a gap recorded only in a report is invisible to anyone
    not reading that report.
    """
    out = _problems(
        _report(("F1", verdict, "no presence detection, see the design doc"))
    )
    assert "must cite the issue" in out


def test_a_positive_result_under_an_open_verdict_is_rejected():
    """THE mpsk mistake, which shipped for one commit.

    `CONFIRMED` means a confirmed DEFECT. Filing a result that HOLDS
    under it made a clean object advertise three open findings against
    one real one. The citation rule catches it for a reason worth
    stating: a result that holds has no issue to cite, because there is
    nothing to fix.
    """
    out = _problems(
        _report(
            (
                "F1",
                "CONFIRMED",
                "The header's ~2x figure is confirmed correct.",
            )
        )
    )
    assert "must cite the issue" in out
    assert "not a finding" in out


@pytest.mark.parametrize("verdict", ["BY DESIGN", "FIXED", "C-ONLY"])
def test_a_closed_verdict_needs_no_issue(verdict):
    """The complement, so the rule cannot creep into closed findings.

    Most findings are `BY DESIGN` or `FIXED` and have nothing to file —
    demanding a link from them is the failure mode `changelog-check`'s
    comment warns about, a gate that argues with its author.
    """
    assert (
        _problems(_report(("F1", verdict, "explained in full, no link"))) == ""
    )


# ── the pre-existing section check still holds ───────────────────────


def test_a_section_reference_that_does_not_resolve_is_rejected():
    """Regression guard for the check that caught a real edit.

    Correcting mpsk's verdicts cited the DESIGN doc's "§9.5"; inside a
    report `§x.y` means that report's own section, and the render was
    refused until the reference named the document instead.
    """
    r = _report(("F1", "FIXED", "corrected"))
    r.md("see §9.5 for the derivation")
    assert "§9.5 is referenced" in _problems(r)


def test_a_coherent_report_renders():
    """Vacuity guard: the cases above must fail for their own reason."""
    r = _report(
        ("F1", "GAP", "unfixed, gh-747"),
        ("F2", "BY DESIGN", "intended"),
    )
    r.summary()
    assert _problems(r) == ""
    assert len(r.open_findings) == 1


# ── section 4 must STATE the envelope, not only count it ──────────────


def test_summary_renders_one_row_per_limit():
    """`summary()` is what closes section 4, so it owns the table.

    Seven of the eleven certified objects rendered section 4 as a heading
    and a sentence with no rows at all, while section 5 beside it closed
    with `N/N limits hold`. Neither gate could see it: the limits test
    never reads the report, and `make validate-check` re-renders and
    compares bytes, so an empty section agrees with itself perfectly.
    """
    r = _report(("F1", "BY DESIGN", "intended"))
    r.limit(True, "a claim that holds")
    r.limit(False, "one that does not")
    r.summary()
    text = r.render()
    assert "| PASS | a claim that holds |" in text
    assert "| **FAIL** | one that does not |" in text
    # The table closes section 4, so it must precede section 5's heading.
    assert text.index("| PASS |") < text.index("## 5. Summary")


def test_recorded_limits_that_reach_no_table_are_rejected():
    """The shipped defect, reproduced: limits counted but never stated.

    Built by hand rather than through `summary()`, because `summary()` is
    the fix — this is what every report looked like before it.
    """
    r = _report(("F1", "BY DESIGN", "intended"))
    r.limit(True, "a claim nobody can read")
    r.md()
    r.md("## 5. Summary")
    r.md()
    r.md("- **1/1 limits** hold")
    assert "renders 0 limit rows against 1 recorded" in _problems(r)


def test_a_report_asserting_no_limits_needs_no_table():
    """Vacuity guard for the case above, from the other side.

    A report with an empty envelope is a different problem — `executive()`
    already calls it NOT CERTIFIED — and it must not be reported as a
    missing table.
    """
    r = _report(("F1", "GAP", "unfixed, gh-747"))
    r.summary()
    assert _problems(r) == ""


# ── what --check gates: structure, not measurements ───────────────────


def _rep(body: str) -> str:
    """A minimal rendered report carrying `body`.

    Carries real `### 2.1` / `### 2.2` headings so a `§2.x` reference in
    `body` resolves — otherwise `_self_check` refuses the render and the
    case fails for the wrong reason, which is how the section-reference
    case first failed.
    """
    r = _report(("F1", "GAP", "unfixed, gh-747"))
    r.md("## 2. Characterisation")
    r.md("### 2.1 One")
    r.md("### 2.2 Two")
    r.md(body)
    r.limit(True, "a claim about 1.234e-02 at 8 dB")
    r.summary()
    return r.render()


def test_measurement_drift_is_not_staleness():
    """The real toolchain difference this exists for, verbatim.

    Measured gcc 15.2/glibc 2.43 against gcc 13.3/glibc 2.39 on one CPU:
    `carrier_nda`'s already-converged frequency error moved from -3.45e-08
    to -1.22e-09 — a 96.5% relative change in a quantity whose value is
    "zero". Byte-comparison called that staleness and sent the reader to
    `make validate`, which on another machine moves the problem rather
    than fixing it.
    """
    a = _rep("| 2 | 0.001000 | -3.45e-08 | -2.3e-11 |")
    b = _rep("| 2 | 0.001000 | -1.22e-09 | -2.1e-11 |")
    assert a != b, "the fixture must differ, or this passes vacuously"
    assert _structural(a) == _structural(b)
    n, worst = _numeric_drift(a, b)
    assert n == 2 and worst > 0.9


@pytest.mark.parametrize(
    ("name", "before", "after"),
    [
        ("prose", "the loop settles", "the loop diverges"),
        ("a table column", "| M | lock |", "| M | lock | extra |"),
        ("a section reference", "see §2.1", "see §2.2"),
        ("an issue citation", "filed as gh-733", "filed as gh-999"),
        ("a hash citation", "tracked by #781", "tracked by #999"),
    ],
)
def test_structural_change_is_staleness(name, before, after):
    """Everything `validate.py` DETERMINES must still be byte-gated.

    Section references and issue citations are the subtle ones: they are
    structure written with digits, so a masker that treats every digit as
    a measurement lets a re-pointed citation through. An earlier version
    did exactly that — it numbered its own placeholders, `_NUMBER` masked
    the indices, and both cases silently passed.
    """
    assert _structural(_rep(before)) != _structural(_rep(after)), name


def test_a_dropped_limit_is_staleness():
    """The claim TEXT is code, even though it embeds measured numbers."""
    keep = _report(("F1", "GAP", "unfixed, gh-747"))
    keep.limit(True, "the first claim")
    keep.limit(True, "the second claim")
    keep.summary()
    drop = _report(("F1", "GAP", "unfixed, gh-747"))
    drop.limit(True, "the first claim")
    drop.summary()
    assert _structural(keep.render()) != _structural(drop.render())


def test_a_changed_verdict_is_staleness():
    """A verdict is a judgement, so it is never measurement noise."""
    gap = _report(("F1", "GAP", "unfixed, gh-747"))
    gap.summary()
    fixed = _report(("F1", "FIXED", "repaired here"))
    fixed.summary()
    assert _structural(gap.render()) != _structural(fixed.render())
