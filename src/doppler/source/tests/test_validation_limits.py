"""The validation reports' certified envelope, as tests that actually run.

Each object under `validation/` ends its report with a **Limits** section
headed "Claims a caller may rely on. A failure here is a regression, not
a new finding." That sentence was false for the first two objects: the
tree was executed by nothing — no make target, no CI job, and
`pytest --collect-only` found zero tests in it — so 44 claims were
asserted by nobody, and two regressions passed straight through them in
one afternoon.

This module is the fix, and it is deliberately thin: it runs each
object's own `build(write=False)` and asserts every limit that run
recorded. The evidence and the gate are therefore the *same code*, so
they cannot disagree the way a hand-copied assertion would.

`write=False` suppresses `results.md`, the plots and the CSVs — a test
must not write into the repo — while every measurement still executes,
so the limits are genuinely exercised rather than replayed.

Staleness of the committed `results.md` is a different question and has
its own gate: `validate.py --check`, run by `make docs-drift-check`.
"""

from __future__ import annotations

import pytest

from doppler.source.tests.validation.lo import validate as lo_validate
from doppler.source.tests.validation.nco import validate as nco_validate
from doppler.tests._validation_common import assert_renders

OBJECTS = {"nco": nco_validate, "lo": lo_validate}


@pytest.fixture(scope="module", params=sorted(OBJECTS))
def report(request):
    """One object's measured report, built once and shared."""
    return OBJECTS[request.param].build(write=False)


def test_every_limit_holds(report):
    """No claim in the certified envelope may fail.

    Reported all at once rather than one assert per limit: when a shared
    primitive moves, several usually go together, and the set is the
    diagnosis.
    """
    failed = list(report.failures())
    assert not failed, "\n".join(
        [f"{len(failed)}/{len(report.limits)} limits FAILED:", *failed]
    )


def test_the_envelope_is_not_empty(report):
    """Guard against a build that silently stops asserting.

    Without this, deleting the limits phase — or an early return inside
    it — would leave `test_every_limit_holds` passing vacuously on an
    empty list, which is exactly the failure this module exists to end.
    """
    assert len(report.limits) >= 15
    assert report.findings, "a report with no findings has not been reviewed"


def test_the_report_renders_coherently(report):
    """The coherence gate, applied to a real report inside CI.

    `_self_check` runs from `render()`, which a `write=False` build never
    reaches, and the two targets that do reach it -- `make validate` and
    `make validate-check` -- are in NO CI workflow. So a report could
    point at a section it does not have, or count limits it never
    rendered, and every CI job stayed green. Free to fix here: this
    module already builds the report, so rendering it is string work on
    data already in memory. See `assert_renders`.
    """
    assert_renders(report)
