"""The hand-typed-version gate, at the one decision it makes per line.

`scripts/check_version_strings.py` refuses doppler's current version typed
into prose. It first misfired at the v0.58.0 release: a docs row quoted
"a jm 0.58.0+ test scaffold" -- just-makeit's version, which doppler's had
climbed to meet. A number labelled as jm's is not a claim about doppler.
"""

from __future__ import annotations

import importlib.util

import pytest

from doppler.tests._repo import repo_root

_SPEC = importlib.util.spec_from_file_location(
    "check_version_strings",
    repo_root(__file__) / "scripts" / "check_version_strings.py",
)
assert _SPEC is not None and _SPEC.loader is not None
gate = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(gate)

V = "0.58.0"


@pytest.mark.parametrize(
    "line",
    [
        "doppler 0.58.0 adds a thing",
        "pip install doppler-dsp==0.58.0",
        "`0.58.0`",
        # A jm mention elsewhere on the line does not excuse doppler's own.
        "doppler 0.58.0, built with jm 0.92.2",
        # The sentence-ending dot is not a longer number (doppler#1943):
        # "the current version is X." is the claim the gate exists for.
        "The current release is 0.58.0.",
        "The current release is 0.58.0.\n",
        # A hyphenated word is prose, not a pre-release label.
        "the 0.58.0-based layout",
    ],
)
def test_doppler_version_in_prose_is_refused(line):
    assert gate.claims_version(line, V)


@pytest.mark.parametrize(
    "line",
    [
        "a jm 0.58.0+ test scaffold",
        "pin just-makeit==0.58.0",
        "just-makeit v0.58.0 shipped it",
        "10.58.01 is a different number",
        "0.58.01 is too",
        "0.58.0.1 is a longer one",
        # A SemVer pre-release label names a run or a candidate, not the
        # release: the v0.66.0 tag tripped on a measurement record that cites
        # the label its bench ran under.
        "`make bench-interleaved VERSION=0.58.0-a4 K=5` at c1ae84190",
        "(`benchmarks/published/v0.58.0-a4/`) stays on the machine",
        "cut 0.58.0-rc1 first",
    ],
)
def test_another_tools_version_or_a_longer_number_passes(line):
    assert not gate.claims_version(line, V)
