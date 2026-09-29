"""Every surface row survives CLI -> ``--record`` -> CLI unchanged.

The surface table (``wfm/wfm_surface.h``, generated from ``just-makeit.toml``
by ``scripts/gen_wfm_defaults.py``) gives each source and segment field a
flag and a scene key. This holds the two to be one field, per row:

1. run ``wfmgen`` with the row's flag at a NON-default value and
   ``--record`` the run;
2. check the record carries that value under the row's key -- the flag
   reached the field, and the writer wrote it;
3. rebuild the flags from the record's table keys alone (the inverse
   mapping), run again, and require the SAME record: a fixed point;
4. feed the record back through ``--from-file`` -- the JSON READER -- and
   require the same record again.

The rows are read from the generator, not listed here, so a row added to the
manifest is tested on the commit that adds it. A row the table owns but this
test cannot drive has to be named in ``SKIP`` with its reason.
"""

from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
from typing import TYPE_CHECKING, Any

import pytest

from doppler.tests._repo import repo_root
from doppler.wfm import cli

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

SCRIPTS = repo_root(__file__) / "scripts"


def _generator() -> ModuleType:
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location(
            "gen_wfm_defaults", SCRIPTS / "gen_wfm_defaults.py"
        )
        assert spec is not None and spec.loader is not None
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.remove(str(SCRIPTS))


GEN = _generator()
ROWS = [
    r
    for r in GEN.surface_rows(
        GEN.tomllib.loads(GEN.MANIFEST.read_text(encoding="utf-8"))
    )
    if r["cli"] and r["json"]
]

#: A non-default value per row where the kind's generic one will not do.
VALUE = {
    "level": "-0.25",
    "fs": "2.0",
    "rrc_beta": "0.5",
    "symbol_rate": "0.05",
    "carrier_hz": "1000000.0",
    "pn_length": "9",
    # Decimal: the CLI reads a hex mask as 0 today (doppler#1611).
    "pn_poly": "24577",
    # Field rows take their CANONICAL text (dp_wfm_field_format), so the
    # record carries back exactly what was given.
    "bits": "0x1acf",
    "acq_code": "pn:31:5*2",
    "sync": "pn:63:6",
    "data_code": "pn:7:3:0x1",
}

#: Flags a row needs beside its own for the run to be buildable at all.
EXTRA = {
    "modulation": ["--bits", "1011"],
    "symbol_rate": ["--data-code", "1011"],
    # A payload is written for a source that carries one; a preamble or a
    # sync word only for a FRAMED one, which needs a payload; a spreading
    # code only for dsss.
    "bits": ["--type", "bits"],
    "acq_code": ["--type", "bits", "--bits", "0x1acf"],
    "sync": ["--type", "bits", "--bits", "0x1acf"],
    "data_code": ["--type", "dsss", "--bits", "0x1acf"],
}

#: Rows this test cannot drive, each with the reason.
SKIP: dict[str, str] = {}


def _value(r: dict[str, Any]) -> str:
    if r["name"] in VALUE:
        return str(VALUE[r["name"]])
    if r["values"]:
        return next(v for v in r["values"] if v != r["default"])
    if r["kind"] == "WFM_SV_DOUBLE":
        return "0.125"
    return "3"


def _flags(r: dict[str, Any], value: str) -> list[str]:
    """The row's flag, plus whatever makes it written: a `json_when`
    condition (`f_end` only exists on a chirp) and any EXTRA."""
    argv = [r["cli"], value]
    w = r["json_when"]
    if w:
        ((field, choice),) = w.items()
        cond = next(
            c for c in ROWS if c["owner"] == r["owner"] and c["name"] == field
        )
        argv += [cond["cli"], choice]
    return argv + EXTRA.get(r["name"], [])


def _record(tmp: Path, argv: list[str]) -> dict[str, Any]:
    rec = tmp / "record.json"
    rec.unlink(missing_ok=True)
    subprocess.run(
        # The harness's own --count goes FIRST, so a row's flag (--count
        # itself) overrides it rather than being overridden.
        # The samples go to the null device: only the record is compared,
        # and a broken build that misreads --count once wrote 8 GB here.
        [
            cli._runnable(),
            "--count",
            "256",
            *argv,
            "--record",
            str(rec),
            "--output",
            os.devnull,
        ],
        check=True,
        capture_output=True,
    )
    return json.loads(rec.read_text(encoding="utf-8"))


def _replay(tmp: Path, record: dict[str, Any]) -> dict[str, Any]:
    scene = tmp / "scene.json"
    scene.write_text(json.dumps(record), encoding="utf-8")
    return _record(tmp, ["--from-file", str(scene)])


def _rebuild(record: dict[str, Any]) -> list[str]:
    """Flags for every table key in a one-source record's segment."""
    seg = record["segments"][0]
    argv: list[str] = []
    for r in ROWS:
        v = seg.get(r["json"])
        if v is None:
            continue
        if isinstance(v, list):
            v = f"{v[0]}:{v[1]}"
        argv += [r["cli"], str(v)]
    return argv


def _key(record: dict[str, Any], r: dict[str, Any]) -> Any:
    return record["segments"][0].get(r["json"])


@pytest.mark.parametrize("row", ROWS, ids=[r["name"] for r in ROWS])
def test_row_reaches_the_record_and_back(row: dict, tmp_path: Path) -> None:
    if row["name"] in SKIP:
        pytest.skip(SKIP[row["name"]])
    value = _value(row)
    first = _record(tmp_path, _flags(row, value))

    got = _key(first, row)
    assert got is not None, f"{row['cli']} {value} wrote no `{row['json']}`"
    if row["values"] or row["kind"] == "WFM_SV_FIELD":
        assert got == value
    else:
        assert float(got) == float(value)

    # The non-table flags (EXTRA) are re-given as they were; every table
    # flag comes back from the record alone.
    again = _record(tmp_path, _rebuild(first) + EXTRA.get(row["name"], []))
    assert again == first
    assert _replay(tmp_path, first) == first


@pytest.mark.parametrize(
    "row",
    [r for r in ROWS if r["range_bit"]],
    ids=[r["name"] for r in ROWS if r["range_bit"]],
)
def test_ranged_row_records_its_span(row: dict, tmp_path: Path) -> None:
    lo, hi = ("-6.0", "-3.0") if row["name"] == "level" else ("2", "5")
    first = _record(tmp_path, _flags(row, f"{lo}:{hi}"))
    assert [float(x) for x in _key(first, row)] == [float(lo), float(hi)]
    again = _record(tmp_path, _rebuild(first) + EXTRA.get(row["name"], []))
    assert again == first
    assert _replay(tmp_path, first) == first


def test_every_row_is_driven() -> None:
    """The table is read, not listed -- but an empty read would pass every
    case above vacuously."""
    assert len(ROWS) >= 20
    assert not set(SKIP) - {r["name"] for r in ROWS}, "stale SKIP entry"
