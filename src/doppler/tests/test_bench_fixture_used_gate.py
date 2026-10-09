"""A benchmark test that takes the fixture and never calls it is an error.

`jm apply` scaffolds ``bench_<obj>.py`` with the ``benchmark`` fixture and
nothing that calls it, and a test written that way passes, collects, and
records no measurement at all (#1010). pytest-benchmark already detects the
shape -- it warns ``Benchmark fixture was not used at all in this test!`` at
fixture teardown -- so the gate is that warning promoted to an error by
``filterwarnings`` in ``pyproject.toml``: every test that requests the
fixture is covered the moment it exists, with nothing to register.

Each case runs a real pytest in a subprocess against a seeded file, with the
REPO's own ``pyproject.toml`` as its config, so what is exercised is the
committed filter and not a restatement of it. The last case is the sabotage:
the same hollow test under a config WITHOUT the filter passes, so it is the
filter that makes the first case red.
"""

from __future__ import annotations

import os
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)

HOLLOW = "def test_hollow(benchmark):\n    pass\n"
CALLS = "def test_calls(benchmark):\n    benchmark(sum, [1, 2, 3])\n"
# The shape a static per-test scan would wrongly refuse: the fixture is
# handed to a helper, which calls it. bench_ratesync.py and bench_corr2d.py
# are written this way, so the gate must accept it.
HELPER = (
    "def _measure(benchmark):\n"
    "    benchmark(sum, [1, 2, 3])\n\n"
    "def test_via_helper(benchmark):\n"
    "    _measure(benchmark)\n"
)

#: ``make test-python`` runs the benchmark tests with timing disabled;
#: ``make bench-python`` runs them timed. The gate must hold in both.
MODES = [["--benchmark-disable"], ["--benchmark-only"]]


def _pytest(tmp_path: Path, body: str, config: Path, mode: list[str]):
    test = tmp_path / "bench_seeded.py"
    test.write_text(body)
    # Run under `make test-python` this test is itself in an xdist worker,
    # and pytest-benchmark reads PYTEST_XDIST_WORKER from the environment
    # to decide xdist is active -- which disables timing and refuses
    # --benchmark-only outright. The child is serial, so it must not
    # inherit the parent's worker identity.
    env = {
        k: v for k, v in os.environ.items() if not k.startswith("PYTEST_XDIST")
    }
    return subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            str(test),
            "-c",
            str(config),
            "--rootdir",
            str(tmp_path),
            "-p",
            "no:cacheprovider",
            "-p",
            "no:xdist",
            "-q",
            *mode,
        ],
        cwd=tmp_path,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


@pytest.mark.parametrize("mode", MODES, ids=["disabled", "timed"])
def test_a_hollow_benchmark_test_errors(tmp_path, mode):
    r = _pytest(tmp_path, HOLLOW, REPO / "pyproject.toml", mode)
    out = r.stdout + r.stderr
    assert r.returncode != 0, out
    assert "Benchmark fixture was not used at all" in out, out
    assert "1 error" in out, out


@pytest.mark.parametrize("mode", MODES, ids=["disabled", "timed"])
@pytest.mark.parametrize("body", [CALLS, HELPER], ids=["direct", "helper"])
def test_a_benchmark_test_that_calls_the_fixture_passes(tmp_path, body, mode):
    r = _pytest(tmp_path, body, REPO / "pyproject.toml", mode)
    assert r.returncode == 0, r.stdout + r.stderr


def test_without_the_filter_the_hollow_test_passes(tmp_path):
    """Sabotage: drop the filter line and the same test goes green.

    Proves the red above comes from the committed filter and not from
    something else in the run, so deleting the line is a visible change.
    """
    text = (REPO / "pyproject.toml").read_text(encoding="utf-8")
    line = (
        '    "error:Benchmark fixture was not used at all:'
        'pytest_benchmark.logger.PytestBenchmarkWarning",\n'
    )
    assert text.count(line) == 1, "the filter is not declared exactly once"
    sabotaged = tmp_path / "pyproject.toml"
    sabotaged.write_text(text.replace(line, ""), encoding="utf-8")
    r = _pytest(tmp_path, HOLLOW, sabotaged, ["--benchmark-disable"])
    assert r.returncode == 0, r.stdout + r.stderr
