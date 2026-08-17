"""The validation reports' certified envelope, as tests that actually run.

Each object under `validation/` ends its report with a **Limits** section
headed "Claims a caller may rely on. A failure here is a regression, not
a new finding." That sentence was false for the campaign's first two
objects: the tree was executed by nothing — no make target, no CI job,
and `pytest --collect-only` found zero tests in it — so 44 claims were
asserted by nobody, and two regressions passed straight through them in
one afternoon.

This module is `track`'s copy of the fix, and it is deliberately thin: it
runs each object's own `build(write=False)` and asserts every limit that
run recorded. The evidence and the gate are therefore the *same code*, so
they cannot disagree the way a hand-copied assertion would.

`write=False` suppresses `results.md`, the plots and the CSVs — a test
must not write into the repo — while every measurement still executes,
so the limits are genuinely exercised rather than replayed.

Staleness of the committed `results.md` is a different question and has
its own gate: `validate.py --check`, run by `make validate-check`.

Kept as a per-module file rather than one shared collector because the
validators import their own module's objects: this one pulls in
`doppler.track`, and a single tree-wide collector would import every
module's extension to run any object's limits.
"""

from __future__ import annotations

import pytest

from doppler.tests._validation_common import assert_renders
from doppler.track.tests.validation.carrier_nda import (
    validate as carrier_nda_validate,
)
from doppler.track.tests.validation.loop_filter import (
    validate as loop_filter_validate,
)
from doppler.track.tests.validation.mpsk_receiver import (
    validate as mpsk_receiver_validate,
)
from doppler.track.tests.validation.ratesync import (
    validate as ratesync_validate,
)

OBJECTS = {
    "carrier_nda": carrier_nda_validate,
    "loop_filter": loop_filter_validate,
    "mpsk_receiver": mpsk_receiver_validate,
    "ratesync": ratesync_validate,
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
