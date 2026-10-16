"""The benchmark page reports a C row's time per the unit the row names.

A C benchmark row times one ROUND of ``iterations`` units of work. Before
#2314 the page printed that round's mean as "time/call", so
``acc_f32::step`` -- 65,536 calls of ``step()`` per round -- read 39 µs for
a 0.6 ns call. jm_bench (0.106.0+) names the unit and writes
``stats.mean_per_unit``; the page must print that, labelled with the unit.
A row from the releases that recorded no unit must say it is per round.

These render the real page (``cmd_page``) from fixture snapshots, so a
regression anywhere between the JSON and the markdown table shows up.
"""

from __future__ import annotations

import importlib.util
import json
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPTS = repo_root() / "scripts"

#: acc_f32::step as the v0.71.0 portable set measured it: one round is
#: 65,536 calls taking 39.08 µs, so one call is 0.596 ns.
ROUND_S = 39.08e-6
ITERS = 65536


def _load(name: str):
    """Import `scripts/<name>.py` by path (scripts/ is not a package)."""
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location(
            name, SCRIPTS / f"{name}.py"
        )
        assert spec and spec.loader
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.remove(str(SCRIPTS))


def _c_row(name: str, mean: float, iters: int, extra: dict) -> dict:
    """One jm_bench JSON row; per-unit stats only when a unit is named."""
    stats = {"mean": mean, "iterations": iters, "ops": iters / mean}
    if "unit" in extra:
        stats["mean_per_unit"] = mean / iters
    return {
        "name": name,
        "fullname": f"bench_x_core::{name}",
        "extra_info": extra,
        "stats": stats,
    }


def _page(
    tmp_path: Path, c_rows: list[dict], py_rows: list[dict] | None = None
) -> str:
    """Render the benchmark page for one release holding ``c_rows`` (and
    ``py_rows`` in its Python snapshot)."""
    rel = tmp_path / "published" / "v9.9.9"
    rel.mkdir(parents=True)
    (rel / "portable.json").write_text(
        json.dumps(
            {"machine_info": {"cpu": "fixture"}, "benchmarks": py_rows or []}
        )
    )
    (rel / "portable-c.json").write_text(json.dumps({"benchmarks": c_rows}))
    report = _load("bench_report")
    out = tmp_path / "benchmarks.md"
    report.cmd_page(report.load_published(str(tmp_path / "published")), out)
    return out.read_text(encoding="utf-8")


def _cell(page: str, row: str) -> str:
    """The portable cell of benchmark ``row`` in the rendered table."""
    (line,) = [ln for ln in page.splitlines() if ln.startswith(f"| `{row}`")]
    return line.split("|")[2].strip()


def test_a_call_row_reads_one_call_not_one_round(tmp_path):
    page = _page(
        tmp_path,
        [
            _c_row(
                "step",
                ROUND_S,
                ITERS,
                {"unit": "call", "rate_unit": "calls/s"},
            )
        ],
    )
    assert _cell(page, "step") == "0.60 ns / call"


def test_a_row_with_no_unit_says_it_is_a_round(tmp_path):
    """v0.43.0 to v0.72.0: no unit, so the honest value is the round."""
    page = _page(tmp_path, [_c_row("step", ROUND_S, ITERS, {})])
    assert _cell(page, "step") == "39.08 µs / round of 65,536"


def test_a_one_call_round_with_no_unit_is_a_call(tmp_path):
    page = _page(tmp_path, [_c_row("power", 181.52e-9, 1, {})])
    assert _cell(page, "power") == "181.52 ns / call"


def test_a_sample_row_is_throughput(tmp_path):
    msas = ITERS / ROUND_S / 1e6
    page = _page(
        tmp_path,
        [
            _c_row(
                "steps",
                ROUND_S,
                ITERS,
                {"unit": "sample", "rate_unit": "Sa/s", "MSa_s": msas},
            )
        ],
    )
    assert _cell(page, "steps") == "1.68 GSa/s"
    latency = page.split("### Latency")[1] if "### Latency" in page else ""
    assert "`steps`" not in latency


def test_a_python_row_is_one_call_whatever_its_iterations(tmp_path):
    """pytest-benchmark divides each round by ``iterations`` itself, so a
    Python row's mean is per call even when ``iterations`` is 220 -- as
    v0.72.0's ``bench_acc_q15.py::test_bench_step`` was, at 22.88 ns."""
    row = {
        "name": "test_bench_step",
        "fullname": "src/doppler/benchmarks/bench_acc_q15.py::test_bench_step",
        "extra_info": {},
        "stats": {"mean": 22.88e-9, "iterations": 220, "ops": 1 / 22.88e-9},
    }
    page = _page(tmp_path, [], [row])
    assert _cell(page, "acc_q15::step") == "22.88 ns / call"
