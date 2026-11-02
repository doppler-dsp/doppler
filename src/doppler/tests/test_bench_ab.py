"""`make bench-ab`'s verdicts: the gate a performance PR is held to (#2759).

The worktrees, builds and timed passes need a bench box and minutes, so they
are exercised by hand (the issue's "reproduces #2744" check). What is pinned
here is the part that decides pass or fail, on synthetic snapshots shaped
like `make bench`'s own JSON: the per-pass best round, the tie rule, and
which verdicts fail the run. The module's doctests run too, so a docstring
example cannot drift from the code.
"""

import doctest
import importlib.util
import sys

from doppler.tests._repo import repo_root

SCRIPTS = repo_root() / "scripts"


def _load():
    """bench_ab, by path.

    Every module the import adds to `sys.modules` (bench_interleaved,
    bench_report, ...) is removed again. Left there, it is what
    `inspect.getmodule` resolves those NAMES to, so another test's own
    by-path copy of the same script stops matching its functions and
    `doctest` finds nothing in it: test_bench_core_pinning's doctest runs
    read "attempted 0" on every Python leg of train #2772.
    """
    before = set(sys.modules)
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location(
            "bench_ab", SCRIPTS / "bench_ab.py"
        )
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.remove(str(SCRIPTS))
        for name in set(sys.modules) - before:
            del sys.modules[name]


AB = _load()


def _snap(rows):
    """A `make bench` snapshot: {fullname: best round, seconds}."""
    return {
        "benchmarks": [
            {"fullname": k, "stats": {"min": v}} for k, v in rows.items()
        ]
    }


def _run(a_passes, b_passes):
    return AB.compare(
        AB.best_rounds([_snap(p) for p in a_passes]),
        AB.best_rounds([_snap(p) for p in b_passes]),
    )


def test_the_module_doctests_pass():
    failed, tried = doctest.testmod(AB)
    assert tried > 0 and failed == 0


def test_noise_within_the_spread_is_a_tie():
    """Same code, run-to-run noise: every row ties, nothing fails."""
    a = [{"r": 1.00, "s": 5.0}, {"r": 1.03, "s": 5.1}, {"r": 1.01, "s": 4.9}]
    b = [{"r": 1.02, "s": 5.0}, {"r": 1.00, "s": 5.2}, {"r": 1.04, "s": 4.95}]
    res = _run(a, b)
    assert {v["verdict"] for v in res.values()} == {"TIE"}
    assert AB.failures(res) == []


def test_one_row_slowed_in_b_fails_that_row_only():
    """The sabotage the issue asks for: one row 10% slower in B, every
    other row unchanged. That row, and only it, fails."""
    a = [{"r": 1.00, "s": 5.0}, {"r": 1.01, "s": 5.01}, {"r": 1.00, "s": 5.0}]
    b = [{"r": 1.10, "s": 5.0}, {"r": 1.11, "s": 5.01}, {"r": 1.10, "s": 5.0}]
    res = _run(a, b)
    assert res["r"]["verdict"] == "SLOWER"
    assert res["s"]["verdict"] == "TIE"
    assert AB.failures(res) == ["r"]


def test_a_faster_b_passes():
    a = [{"r": 2.0}, {"r": 2.01}]
    b = [{"r": 1.0}, {"r": 1.01}]
    res = _run(a, b)
    assert res["r"]["verdict"] == "FASTER" and AB.failures(res) == []


def test_a_row_b_never_measured_fails():
    """A gate cannot pass on a row it never saw: B dropping one is red."""
    res = _run([{"r": 1.0, "gone": 1.0}] * 2, [{"r": 1.0}] * 2)
    assert res["gone"]["verdict"] == "MISSING"
    assert AB.failures(res) == ["gone"]


def test_a_new_row_in_b_is_reported_not_failed():
    res = _run([{"r": 1.0}] * 2, [{"r": 1.0, "added": 3.0}] * 2)
    assert res["added"]["verdict"] == "NEW"
    assert AB.failures(res) == []


def test_the_tie_rule_uses_the_larger_spread():
    """A noisy A cannot be beaten by a quiet B within A's own noise."""
    v = AB.verdict([1.0, 1.5], [1.4, 1.4])
    assert v["spread"] == 0.5 and v["verdict"] == "TIE"
