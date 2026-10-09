"""Rule 6 of ``scripts/check_bench_coverage.py``: a published throughput is
one a kernel can reach (#1918).

The corr benchmark gave a 64-point kernel 65 536 samples and credited the
call with all of them, so ``docs/benchmarks.md`` printed 206 GSa/s. Every
other rule of that script looked at the file, the target, the row or the
name, and all of them held. These cases seed a fake ``published/`` tree,
because a gate that only runs against the real one cannot be shown to go red
without corrupting the real numbers.
"""

from __future__ import annotations

import importlib.util
import json
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPTS = repo_root(__file__) / "scripts"


def _gate():
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location(
            "check_bench_coverage_under_test",
            SCRIPTS / "check_bench_coverage.py",
        )
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.remove(str(SCRIPTS))


def _snapshot(root: Path, version: str, build: str, rows: dict) -> None:
    d = root / version
    d.mkdir(parents=True, exist_ok=True)
    (d / f"{build}.json").write_text(
        json.dumps(
            {
                "benchmarks": [
                    {
                        "fullname": f"src/doppler/x/benchmarks/{name}",
                        "extra_info": {"MSa_s": v},
                    }
                    for name, v in rows.items()
                ]
            }
        )
    )


def test_a_figure_above_the_ceiling_is_reported(tmp_path):
    g = _gate()
    _snapshot(
        tmp_path,
        "v1.0.0",
        "portable",
        {"bench_a.py::fast": 206_000.0, "bench_b.py::ok": 30_000.0},
    )
    assert g.implausible(tmp_path) == {"bench_a.py::fast": 206_000.0}


def test_the_higher_of_portable_and_native_is_reported(tmp_path):
    g = _gate()
    _snapshot(tmp_path, "v1.0.0", "portable", {"bench_a.py::t": 120_000.0})
    _snapshot(tmp_path, "v1.0.0", "native", {"bench_a.py::t": 262_000.0})
    assert g.implausible(tmp_path) == {"bench_a.py::t": 262_000.0}


def test_only_the_newest_snapshot_counts(tmp_path):
    """Old releases are history: a fixed benchmark must be able to leave."""
    g = _gate()
    _snapshot(tmp_path, "v0.9.0", "portable", {"bench_a.py::t": 500_000.0})
    _snapshot(tmp_path, "v0.10.0", "portable", {"bench_a.py::t": 20_000.0})
    assert g.implausible(tmp_path) == {}


def test_a_row_with_no_throughput_is_not_a_figure(tmp_path):
    g = _gate()
    d = tmp_path / "v1.0.0"
    d.mkdir()
    (d / "portable.json").write_text(
        json.dumps({"benchmarks": [{"fullname": "bench_a.py::lat"}]})
    )
    assert g.implausible(tmp_path) == {}


def test_the_waiver_list_is_exactly_what_the_real_snapshot_holds():
    """Both directions: a new over-ceiling figure is unwaived (the gate
    fails), and a waiver for a figure that has gone is stale (it fails)."""
    g = _gate()
    assert set(g.implausible()) == set(g.IMPLAUSIBLE_ALLOW)
