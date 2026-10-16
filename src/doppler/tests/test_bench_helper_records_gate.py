"""Rule 5 of ``scripts/check_bench_coverage.py`` follows an imported helper.

Rule 5 refuses a ``bench_*.py`` that never calls the ``benchmark`` fixture.
The burst chain's benches time through one shared helper,
``on_fresh(benchmark, make, call)`` in ``_burst_stimulus.py``, which calls
``benchmark.pedantic``. When the DSSS bench cut (#2386) left
``bench_burst_capture.py`` with only rows that go through it, rule 5
called a file that records "hollow". These cases seed a fake ``src/doppler``
tree, so the helper's own body decides, not its name.
"""

from __future__ import annotations

import importlib.util
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPTS = REPO / "scripts"

BENCH = (
    "from doppler.x._helper import measure\n\n"
    "def test_row(benchmark):\n"
    "    measure(benchmark, sum)\n"
)
RECORDS = (
    "def measure(benchmark, fn):\n"
    "    return benchmark.pedantic(fn, rounds=1)\n"
)
HOLLOW = "def measure(benchmark, fn):\n    return fn([1])\n"


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


def _seed(tmp_path: Path, helper: str):
    src = tmp_path / "src" / "doppler"
    (src / "x" / "benchmarks").mkdir(parents=True)
    (src / "x" / "_helper.py").write_text(helper)
    bench = src / "x" / "benchmarks" / "bench_x.py"
    bench.write_text(BENCH)
    gate = _gate()
    gate.SRC = src
    return gate, bench


def test_a_helper_that_calls_the_fixture_records(tmp_path):
    gate, bench = _seed(tmp_path, RECORDS)
    assert gate._py_records(bench)


def test_a_helper_that_never_calls_it_does_not(tmp_path):
    """The hollow shape one file over: handed the fixture, never calls it."""
    gate, bench = _seed(tmp_path, HOLLOW)
    assert not gate._py_records(bench)


def test_the_burst_capture_bench_records():
    """The file that found this: every row goes through ``on_fresh``."""
    gate = _gate()
    bench = REPO / "src/doppler/acquire/benchmarks/bench_burst_capture.py"
    assert gate._py_records(bench)
