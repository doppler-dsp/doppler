"""The certified envelope of the components that have no Python face.

Every other `test_validation_limits.py` lives beside its module and
imports that module's objects. This one is tree-wide and imports none,
because the objects it gates — `conv`, `rs` and `ccsds_tm` — have no
binding: the
measurements come from a C harness under `native/validation/`, and the
validator beside this file renders and asserts them.

That is the whole difference. The rest is the campaign's rule unchanged:
run the object's own `build(write=False)` and assert every limit that run
recorded, so the evidence and the gate are the *same code* and cannot
disagree the way a hand-copied assertion would.

**It fails rather than skips when the C harness is missing.** A skipped
measurement and a passing one look identical in a log, and this whole
file is about numbers that would otherwise be checked nowhere; `make
build` builds the harness, and CI builds before it runs any Python.

Staleness of the committed `results.md` is a different question with its
own gate: `validate.py --check`, run by `make validate-check`.
"""

from __future__ import annotations

import pytest

from doppler.tests._validation_common import assert_renders
from doppler.tests.validation.ccsds_tm import validate as ccsds_tm_validate
from doppler.tests.validation.conv import validate as conv_validate
from doppler.tests.validation.rs import validate as rs_validate

OBJECTS = {
    "ccsds_tm": ccsds_tm_validate,
    "conv": conv_validate,
    "rs": rs_validate,
}


@pytest.fixture(scope="module", params=sorted(OBJECTS))
def report(request):
    """One component's measured report, built once and shared."""
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
    assert len(report.limits) >= 6
    assert report.findings, "a report with no findings has not been reviewed"


def test_the_report_renders_coherently(report):
    """The coherence gate, applied to a real report inside CI."""
    assert_renders(report)
