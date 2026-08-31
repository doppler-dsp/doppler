"""The certified envelope of the components that have no Python face.

Every other `test_validation_limits.py` lives beside its module and
imports that module's objects. This one is tree-wide and imports none,
because the subjects it gates have no binding: `conv`, `rs` and
`ccsds_tm` are components with no Python face, and `wfmgen` is a TOOL
with no header either. In every case the measurements come from a
harness under `native/validation/`, and the validator beside this file
renders and asserts them.

**`OBJECTS` is DISCOVERED, not written down** (doppler#1144).
`docs/dev/contributing/validation.md` says both gates "discover by glob, so
a new object is gated the moment its folder exists". That was true of
`make validate-check` and false here: this file carried a hand-written dict,
so a validator added under `src/doppler/tests/validation/` rendered a
report, passed the staleness gate AND the report-format gate, and had its
limits asserted by nobody until someone remembered to edit it. wfmgen sat
exactly there for the length of one commit -- three green gates and an
unasserted envelope, which is the shape that page's own opening section
describes.

Discovery is bounded and cheap: this directory holds only the subjects with
no Python face, so importing them costs what the named ones already cost.
`_discover()` fails loudly on an empty result, because a glob that silently
matches zero is the same defect one level up.

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

import importlib
from pathlib import Path
from typing import TYPE_CHECKING

import pytest

from doppler.tests._validation_common import assert_renders

if TYPE_CHECKING:
    from types import ModuleType

_HERE = Path(__file__).resolve().parent


def _discover() -> dict[str, ModuleType]:
    """Every validator beside this file, by folder name.

    A folder is a subject iff it holds `validate.py`, so `__pycache__` and
    any stray directory drop out without needing to be named.
    """
    found = {
        d.name: importlib.import_module(
            f"doppler.tests.validation.{d.name}.validate"
        )
        for d in sorted(_HERE.glob("validation/*/"))
        if (d / "validate.py").is_file()
    }
    if not found:
        raise AssertionError(
            f"no validators found under {_HERE / 'validation'} - a gate that "
            "matches nothing reports a clean tree, which is the defect this "
            "file exists to end (doppler#1144)"
        )
    return found


OBJECTS = _discover()


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
