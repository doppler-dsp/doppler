"""Every lock flag states how its rule was sized, and the header says so
(doppler#3230).

Three roles, and this holds each to its own:

* the detection module (``dp_det_*``) holds every equation and statistical
  model that turns rates into a threshold, a non-coherent count, a verify
  count, a block-averaging length or an averaging weight;
* ``LockDet`` applies thresholds and counts and knows nothing else;
* the application chooses the rates for its use, calls detection with them
  and its own signal parameters, and hands the results to ``LockDet``.

The record of the rates is the application's HEADER: on the configure call
or the constant that sizes the rule it names the detection call that
produces each number. This test is the doc half: for each flag it reads what
the code calls and requires the header to name it. A flag whose rule is a
literal is on ``GAPS`` with the issue that records it; the list only shrinks
(an entry for a flag that has since been derived fails, so it is deleted).
Terms are the correlator page's: coherent dwell, non-coherent count, total
dwell (one threshold decision per update of the statistic).
"""

from __future__ import annotations

import re
import sys

from doppler.tests._repo import repo_root

if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover
    import tomli as tomllib

import pytest

ROOT = repo_root(__file__)
INC = "native/inc/doppler"
SRC = "native/src"

#: Flags whose rule is derived or declared, with where the code calls
#: detection and where the header must say so. ``calls`` are the
#: ``dp_det_*`` calls the source makes (read from the file, not typed here:
#: the test fails if the source calls one the header does not name).
#: ``literal_in_code``: the numbers are literals that a test recomputes from
#: the detection call, so the header names the call and the rates and
#: ``recomputed_by`` holds the arithmetic.
DERIVED = {
    "dll": {
        "source": f"{SRC}/dll/dll_core.c",
        "header": f"{INC}/dll/dll_core.h",
    },
    "symsync": {
        "source": f"{SRC}/symsync/symsync_core.c",
        "header": f"{INC}/symsync/symsync_core.h",
    },
    "async_dsss_receiver": {
        "source": f"{SRC}/async_dsss_receiver/async_dsss_receiver_core.c",
        "header": f"{INC}/async_dsss_receiver/async_dsss_receiver_core.h",
    },
}
LITERAL_BUT_STATED = {
    "ratesync": {
        "header": f"{INC}/ratesync/ratesync_core.h",
        "names": [
            "dp_det_verify_count",
            "RATESYNC_LOCK_PFA",
            "RATESYNC_LOCK_PD",
        ],
        "recomputed_by": "native/tests/ratesync/test_ratesync_core.c",
    },
    "carrier_nda": {
        "header": f"{INC}/carrier_nda/carrier_nda_core.h",
        "names": ["det_ema_alpha", "Pfa"],
        "recomputed_by": None,
    },
}

#: Flags with no stated rates, shrink-only: object -> why, with the issue.
GAPS = {
    "costas": "literal thresholds and counts, no rates stated (#3252)",
    "mpsk_receiver": (
        "threshold stated as a typed sigma product, counts literal and "
        "shorter than the statistic's memory (#3236)"
    ),
    "dsss_receiver": (
        "a constant threshold of 0.3, given a Pfa reading afterwards (#3237)"
    ),
    "despreader": "its carrier lock is set by hand (#3254)",
    "async_dsss_receiver:symbol": (
        "the symbol-lock rule is literals with no rates (#3253)"
    ),
}

#: A model a derived rule uses that is not detection's: a fit, never a
#: derivation or a committed measurement (card principle 4). Shrink-only.
OUTSIDE_MODELS = {
    "symsync": "the lock statistic's mean is a fit (#3255)",
}

#: Not an application: it applies thresholds and counts.
PRIMITIVE = {"lockdet"}


def _read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def _discover() -> set[str]:
    """Objects that publish a lock decision."""
    found = set()
    for f in sorted((ROOT / "objects").glob("*.toml")):
        for obj, table in tomllib.loads(f.read_text(encoding="utf-8")).items():
            if not isinstance(table, dict):
                continue
            names = [
                p.get("name", "")
                for sec in ("properties", "methods")
                for p in table.get(sec) or []
            ]
            if any(
                re.fullmatch(r"locked|lock|carrier_locked|code_locked", n)
                for n in names
            ):
                found.add(obj)
    return found


def _stem(name: str) -> str:
    return re.sub(r"^dp_", "", name)


def _calls(source: str) -> set[str]:
    """The detection calls a source makes (not its comments' mentions)."""
    code = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    code = re.sub(r"//[^\n]*", "", code)
    return {_stem(m) for m in re.findall(r"\b(dp_det_\w+)\s*\(", code)}


#: Conversions, not a rule's numbers.
NOT_A_RULE = {"det_cn0_to_snr"}


def test_every_lock_flag_is_derived_stated_or_a_declared_gap():
    found = _discover()
    known = (
        set(DERIVED)
        | set(LITERAL_BUT_STATED)
        | {g.split(":")[0] for g in GAPS}
        | PRIMITIVE
    )
    assert found - known == set(), (
        "a lock flag with no row: derive it, state it, or declare the gap "
        f"with its issue: {sorted(found - known)}"
    )
    assert known - found == set(), sorted(known - found)


def test_a_gap_names_its_issue_and_is_not_also_a_row():
    for obj, why in {**GAPS, **OUTSIDE_MODELS}.items():
        assert re.search(r"#\d+", why), (obj, why)
    assert {g.split(":")[0] for g in GAPS} & (
        set(DERIVED) | set(LITERAL_BUT_STATED)
    ) <= {"async_dsss_receiver"}  # its code lock is derived, symbol lock a gap


@pytest.mark.parametrize("obj", sorted(DERIVED))
def test_the_header_names_each_detection_call_the_code_makes(obj):
    row = DERIVED[obj]
    calls = _calls(_read(row["source"])) - NOT_A_RULE
    assert calls, f"{obj}: the source makes no detection call"
    header = _read(row["header"])
    stems = set(re.findall(r"\b(?:dp_)?(det_\w+)", header))
    missing = sorted(c for c in calls if _stem(c) not in stems)
    assert not missing, (
        f"{row['header']} must name the detection calls {obj} makes: {missing}"
    )


@pytest.mark.parametrize("obj", sorted(LITERAL_BUT_STATED))
def test_a_literal_rule_names_its_rates_and_the_call_that_recomputes_it(obj):
    row = LITERAL_BUT_STATED[obj]
    header = _read(row["header"])
    missing = [
        n
        for n in row["names"]
        if not re.search(rf"\b{re.escape(n)}\b", header)
    ]
    assert not missing, (row["header"], missing)
    if row["recomputed_by"]:
        assert "dp_det_verify_count" in _read(row["recomputed_by"])
