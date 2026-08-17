"""The validation reports' certified envelope, as tests that actually run.

`mpsk`'s copy of the campaign's limits gate. Each object under
`validation/` ends its report with a **Limits** section headed "Claims a
caller may rely on. A failure here is a regression, not a new finding" —
and that sentence is only true if something executes it.

Deliberately thin: it runs each object's own `build(write=False)` and
asserts every limit that run recorded, so the evidence and the gate are
the *same code* and cannot disagree the way a hand-copied assertion
would. `write=False` suppresses `results.md` and the CSVs — a test must
not write into the repo — while every measurement still executes.

Staleness of the committed `results.md` is a different question with its
own gate: `validate.py --check`, run by `make validate-check`.

Kept per-module rather than as one tree-wide collector because the
validators import their own module's objects: this one pulls in
`doppler.mpsk`, and a shared collector would import every module's
extension to run any object's limits.
"""

from __future__ import annotations

import pytest

from doppler.mpsk.tests.validation.mpsk import validate as mpsk_validate
from doppler.tests._validation_common import assert_renders

OBJECTS = {
    "mpsk": mpsk_validate,
}


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
    assert len(report.limits) >= 20
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
