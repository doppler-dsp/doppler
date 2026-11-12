"""The pull request half of the bench label gate (doppler#2390).

`make test-python` runs every Python benchmark once, untimed
(``--benchmark-disable``), so it already knows which rows a pull request
measures. ``--bench-rows PATH`` writes them down for
``scripts/bench_report.py --check-pr-rows``. Opt-in: with no path nothing
is recorded and nothing is written, so a plain test run is unchanged.

A row's name is read AFTER the test body, because a bench may rename its
own row (``benchmark.name = f"decimate_R{R}"`` in ``bench_cic.py``). It
travels in ``user_properties``, which xdist carries from each worker to the
controller, and only the controller writes the file. Each entry is the
fixture's own record (``name``, ``fullname``); the gate names it as a
published snapshot would (``bench_report.jm_qualified``), not this file.

Loaded by the root ``conftest.py`` (``pytest_plugins``), and by a test that
wants it alone with ``-p doppler.tests._bench_rows``.
"""

from __future__ import annotations

import json
import pathlib

#: What this process's reports carried; only the controller's is written.
_BENCH_ROWS: list = []


def pytest_addoption(parser):
    parser.addoption(
        "--bench-rows",
        default=None,
        metavar="PATH",
        help="write every benchmark row this run measured to PATH "
        "(the pull request bench label gate, #2390)",
    )


def _wanted(config) -> bool:
    return bool(config.getoption("bench_rows", default=None))


def pytest_runtest_teardown(item, nextitem):
    if not _wanted(item.config):
        return
    bm = getattr(item, "funcargs", {}).get("benchmark")
    if bm is not None:
        item.user_properties.append(
            ("bench_row", {"name": bm.name, "fullname": bm.fullname})
        )


def pytest_runtest_logreport(report):
    # The teardown report is the one that carries what teardown appended.
    if report.when != "teardown":
        return
    for key, value in report.user_properties:
        if key == "bench_row":
            _BENCH_ROWS.append(value)


def pytest_sessionfinish(session, exitstatus):
    config = session.config
    if not _wanted(config) or hasattr(config, "workerinput"):
        return
    path = pathlib.Path(config.getoption("bench_rows"))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({"suite": "python", "benches": _BENCH_ROWS}, indent=1)
        + "\n",
        encoding="utf-8",
    )
