"""The claim scope of ``native/tests/dp_test.h``, through its own binary.

``test_dp_claims`` takes a mode and exits with the status the scope must give;
CTest holds the exit statuses, this reads what the passing modes WRITE (the
records ``DP_CLAIMS_OUT`` collects) and what the failing ones SAY, which
ctest cannot see: a claim that holds a check, a control and a pair is
recorded with the counts it should have, a helper's checks belong to the open
claim, a check outside any claim writes nothing, and each failing mode fails
for the reason it names and not for some other.
"""

from __future__ import annotations

import json
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import build_dir, exe

if TYPE_CHECKING:
    from pathlib import Path

BIN = exe(build_dir() / "test_dp_claims")


def run(mode: str, out: Path) -> subprocess.CompletedProcess[str]:
    if not BIN.exists():
        pytest.fail(f"{BIN} is not built: run `make build` first")
    return subprocess.run(
        [str(BIN), mode],
        capture_output=True,
        text=True,
        env={"DP_CLAIMS_OUT": str(out)},
        check=False,
    )


def records(out: Path) -> list[dict]:
    return [
        json.loads(x) for x in out.read_text(encoding="utf-8").splitlines()
    ]


def test_a_claim_with_a_control_and_a_pair_is_recorded(tmp_path):
    out = tmp_path / "c.jsonl"
    r = run("pass", out)
    assert r.returncode == 0, r.stderr
    got = records(out)
    assert [(c["claim"], c["checks"], c["controls"]) for c in got] == [
        ("doubles", 1, 1),
        ("refuses", 1, 1),
    ]
    # a quote in a claim's text is escaped, and the record parses back to it
    assert got[1]["text"] == 'a "negative" argument is refused'
    assert got[0]["bin"] == "test_dp_claims[pass]"


def test_a_helpers_checks_belong_to_the_open_claim(tmp_path):
    out = tmp_path / "c.jsonl"
    assert run("helper", out).returncode == 0
    (rec,) = records(out)
    assert (rec["claim"], rec["checks"], rec["controls"]) == (
        "from_helper",
        2,
        1,
    )


def test_a_test_that_opens_no_claim_writes_nothing(tmp_path):
    out = tmp_path / "c.jsonl"
    assert run("unscoped", out).returncode == 0
    assert not out.exists()


@pytest.mark.parametrize(
    "mode, says",
    [
        ("control_passes", "passed: the assertion cannot fail"),
        ("pair_refused", "was refused too"),
        ("not_refused", "a refusal that was not refused"),
        ("empty_claim", "holds no check"),
        ("outside", "outside any claim"),
        ("duplicate", "is used twice"),
    ],
)
def test_each_failing_mode_fails_for_the_reason_it_names(tmp_path, mode, says):
    r = run(mode, tmp_path / "c.jsonl")
    assert r.returncode == 1
    assert says in r.stderr, r.stderr
