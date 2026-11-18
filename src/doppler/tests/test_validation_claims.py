"""The report's reading of a C test's claims (`Report.limit(claim_id=...)`)."""

from __future__ import annotations

from pathlib import Path

import pytest

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, claim_records

REC = {
    "bin": "test_x_core",
    "claim": "c",
    "text": "c holds",
    "checks": 4,
    "controls": 2,
}


def test_a_limit_reads_its_claims_counts():
    r = Report(write=False)
    r.use_claims({"c": REC})
    r.limit(True, "c holds", control="the wrong case", claim_id="c")
    (_, control, _) = r.evidence[0]
    assert "the wrong case" in control
    assert "test_x_core" in control and "4 checks, 2 controls" in control


def test_a_claim_the_c_test_never_opened_is_a_hard_failure():
    r = Report(write=False)
    r.use_claims({"c": REC})
    with pytest.raises(SystemExit, match="names no claim"):
        r.limit(True, "c holds", claim_id="other")


def test_a_claim_with_no_control_is_a_hard_failure():
    r = Report(write=False)
    r.use_claims({"c": dict(REC, controls=0)})
    with pytest.raises(SystemExit, match="has no control"):
        r.limit(True, "c holds", claim_id="c")


def test_a_missing_test_binary_is_a_hard_failure(tmp_path):
    with pytest.raises(SystemExit, match="is not built"):
        claim_records(tmp_path / "nope", "x", Path(tmp_path))
    assert repo_root().exists()
