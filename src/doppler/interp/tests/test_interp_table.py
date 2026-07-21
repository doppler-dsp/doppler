"""Tests for the InterpolatedTable Python extension.

Mirrors test_interp_table_core.c: lifecycle, floor/nearest/linear
methods (including the exact-0.5-tie case and wraparound), table
copy-not-alias, no-aliasing-across-calls regression, no-leak-in-a-loop
regression.
"""

import resource

import numpy as np
import pytest

from doppler.interp import InterpolatedTable


def test_create():
    obj = InterpolatedTable(np.zeros(1, dtype=np.complex128), "linear")
    assert obj is not None


def test_create_rejects_empty_table():
    with pytest.raises(MemoryError):
        InterpolatedTable(np.zeros(0, dtype=np.complex128))


def test_create_rejects_bad_method():
    with pytest.raises(ValueError):
        InterpolatedTable(np.zeros(1, dtype=np.complex128), "cubic")


def test_default_method_is_linear():
    obj = InterpolatedTable(np.array([0.0, 1.0, 2.0], dtype=np.complex128))
    np.testing.assert_allclose(obj.execute(np.array([0.5])), [0.5])


def test_n_property():
    obj = InterpolatedTable(np.array([1.0, 2.0, 3.0], dtype=np.complex128))
    assert obj.n == 3


def test_context_manager():
    with InterpolatedTable(np.zeros(1, dtype=np.complex128), "linear"):
        pass


def test_destroy():
    obj = InterpolatedTable(np.zeros(1, dtype=np.complex128), "linear")
    obj.destroy()


# ── floor / nearest / linear (mirrors test_interp_table_core.c) ───────


def test_floor():
    t = InterpolatedTable(
        np.array([10.0, 20.0, 30.0], dtype=np.complex128), "floor"
    )
    out = t.execute(np.array([0.9, 1.9, 2.9]))
    np.testing.assert_allclose(out, [10.0, 20.0, 30.0])


def test_nearest_including_exact_tie():
    """A fraction of exactly 0.5 selects the floor (lower) index."""
    t = InterpolatedTable(
        np.array([10.0, 20.0, 30.0], dtype=np.complex128), "nearest"
    )
    out = t.execute(np.array([0.4, 0.6, 1.5]))
    np.testing.assert_allclose(out, [10.0, 20.0, 20.0])


def test_linear_interior_wraparound_and_negative():
    t = InterpolatedTable(
        np.array([10.0, 20.0, 30.0], dtype=np.complex128), "linear"
    )
    out = t.execute(np.array([0.25, 2.75, -0.5]))
    np.testing.assert_allclose(out, [12.5, 15.0, 20.0])


def test_table_is_copied_not_aliased():
    t = np.array([1.0, 2.0], dtype=np.complex128)
    obj = InterpolatedTable(t, "linear")
    t[0] = 999.0  # mutate the caller's own array after construction
    out = obj.execute(np.array([0.0]))
    np.testing.assert_allclose(out, [1.0])


def test_reference_ramp_example():
    """Matches the working implementation's own worked examples."""
    table = InterpolatedTable(np.array([0.0, 1.0, 2.0], dtype=np.complex128))
    np.testing.assert_allclose(table.execute(np.array([1.1])), [1.1])
    np.testing.assert_allclose(table.execute(np.array([0.5, 1.1])), [0.5, 1.1])


def test_out_writes_into_callers_buffer():
    t = InterpolatedTable(np.array([1.0, 2.0], dtype=np.complex128))
    x = np.array([0.5], dtype=np.float64)
    out = np.zeros(1, dtype=np.complex128)
    y = t.execute(x, out=out)
    assert np.shares_memory(y, out)


def test_out_undersized_raises():
    t = InterpolatedTable(np.array([1.0, 2.0], dtype=np.complex128))
    with pytest.raises(ValueError):
        t.execute(np.zeros(4), out=np.zeros(1, dtype=np.complex128))


# ── No-aliasing / no-leak regressions (jm's default variable_output ───
# ── codegen has both bugs; this fragment was hand-fixed -- see the ────
# ── identical regression tests in test_nco.py / test_lo.py) ───────────


def test_execute_no_aliasing_across_calls():
    t = InterpolatedTable(
        np.array([10.0, 20.0, 30.0], dtype=np.complex128), "linear"
    )
    first = t.execute(np.array([0.25]))
    first_snapshot = first.copy()
    _ = t.execute(np.array([1.75]))
    np.testing.assert_array_equal(first, first_snapshot)


def test_execute_no_leak_in_tight_loop():
    t = InterpolatedTable(
        np.array([10.0, 20.0, 30.0], dtype=np.complex128), "linear"
    )
    x = np.array([0.25, 1.5, 2.75])
    for _ in range(2000):
        _ = t.execute(x)
    start_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    for _ in range(50_000):
        _ = t.execute(x)
    end_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    assert (end_kb - start_kb) < 20_000
