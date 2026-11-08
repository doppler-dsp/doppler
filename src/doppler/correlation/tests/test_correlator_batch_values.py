"""The batch reducers' VALUES, against NumPy.

#2683; docs/design/correlator-python.md §10.

``test_correlator_batch.py`` holds what only the binding can get wrong: the
shapes, the dtypes, ``out=`` handed back as a view, the refusals. These hold
the numbers. Every reference is built from ``execute_all``'s maps, the
unreduced batch, in NumPy, so a reducer is checked against the one call that
does no reducing, and never against itself.

The arithmetic each reducer owes, from ``correlator_core.h``:

- a cell's power is ``|map_h|^2`` formed in double and rounded to float
  once (``corr2d_pow``), so the references square in float64 and cast;
- ``execute_power_sum`` sums those powers in h order, in float: the
  reference folds the same way, so the comparison is bit for bit;
- ``execute_power_max`` keeps the largest power per cell and the h it came
  from; a later map replaces it only when STRICTLY greater, so a tie keeps
  the lowest h. ``np.argmax`` returns the first maximum, the same rule;
- a batch never reads or writes the dwell: ``count``, the accumulator and
  the state blob are what they were, so a batch may run mid-dwell.

Each test was proved by sabotage (its reference broken in a scratch copy,
red, restored); the report that landed this file names each one.
"""

from __future__ import annotations

from collections.abc import Callable

import numpy as np
import pytest

from doppler.correlation import Correlator

NY, NX, H = 4, 16, 5


def _code() -> np.ndarray:
    rng = np.random.default_rng(2683)
    return np.exp(2j * np.pi * rng.random(NX)).astype(np.complex64)


def _frames(n: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    z = rng.standard_normal(n) + 1j * rng.standard_normal(n)
    return z.astype(np.complex64)


def _fast(dwell: int = 1) -> Correlator:
    """The 2-D map on its single-row fast path."""
    ref = np.zeros((NY, NX), np.complex64)
    ref[0] = _code()
    return Correlator(ref=ref, dwell=dwell, nthreads=2)


def _general(dwell: int = 1) -> Correlator:
    """The 2-D map with energy off row 0: the general 2-D path."""
    return Correlator(ref=_general_ref(), dwell=dwell, nthreads=2)


def _col(dwell: int = 1) -> Correlator:
    """The 2-D map reduced to one known column."""
    ref = np.zeros((NY, NX), np.complex64)
    ref[0] = _code()
    return Correlator(ref=ref, dwell=dwell, col_out=3)


def _dd(dwell: int = 1) -> Correlator:
    """The delay-Doppler map, padded in Doppler."""
    return Correlator(
        _code(), doppler_rows=NY, ny_out=2 * NY, dwell=dwell, nthreads=2
    )


def _one_d(dwell: int = 1) -> Correlator:
    """The 1-D map: the 2-D one at one row.

    docs/design/correlator-python.md §18.
    """
    return Correlator(_code(), dwell=dwell)


def _general_ref() -> np.ndarray:
    rng = np.random.default_rng(7)
    return (
        rng.standard_normal((NY, NX)) + 1j * rng.standard_normal((NY, NX))
    ).astype(np.complex64)


def _clean(make: Make) -> np.ndarray:
    """One frame that correlates to a single sharp peak on ``make``'s map:
    the reference's own samples, placed where the map reads them."""
    if make is _general:
        return _general_ref().ravel()  # its own autocorrelation
    if make is _dd:
        return np.tile(_code(), NY)  # NY code periods, zero Doppler
    if make is _one_d:
        return _code()
    frame = np.zeros((NY, NX), np.complex64)  # _fast and _col: row 0 only
    frame[0] = np.roll(_code(), 3)  # delay 3, the column _col keeps
    return frame.ravel()


Make = Callable[..., Correlator]

MAKERS = [
    pytest.param(_fast, id="2d-fast"),
    pytest.param(_general, id="2d-general"),
    pytest.param(_col, id="2d-col_out"),
    pytest.param(_dd, id="delay_doppler"),
    pytest.param(_one_d, id="1d"),
]


def _frame_len(obj: Correlator) -> int:
    return obj.ny * obj.nx


def _powers(maps: np.ndarray) -> np.ndarray:
    """``|map|^2`` per cell, formed in double and rounded to float once."""
    re = maps.real.astype(np.float64)
    im = maps.imag.astype(np.float64)
    return (re * re + im * im).astype(np.float32)


def _power_sum_ref(maps: np.ndarray) -> np.ndarray:
    """The powers summed in h order, in float, as the C reducer folds."""
    p = _powers(maps)
    acc = p[0].copy()
    for row in p[1:]:
        acc = (acc + row).astype(np.float32)
    return acc


# ── power_sum and power_max: values, index and tie rule ──────────────────


@pytest.mark.parametrize("make", MAKERS)
def test_power_sum_is_the_maps_powers_summed_in_h_order(make: Make) -> None:
    obj = make()
    x = _frames(H * _frame_len(obj), seed=11)
    maps = obj.execute_all(x)
    np.testing.assert_array_equal(
        obj.execute_power_sum(x), _power_sum_ref(maps)
    )


@pytest.mark.parametrize("make", MAKERS)
def test_power_max_is_the_largest_power_and_the_map_it_came_from(
    make: Make,
) -> None:
    obj = make()
    x = _frames(H * _frame_len(obj), seed=12)
    p = _powers(obj.execute_all(x))
    power, index = obj.execute_power_max(x)
    np.testing.assert_array_equal(power, p.max(axis=0))
    np.testing.assert_array_equal(index, np.argmax(p, axis=0))
    # Not a vacuous index: the winners come from more than one map.
    assert len(np.unique(index)) > 1


@pytest.mark.parametrize("make", MAKERS)
def test_power_max_gives_a_tie_to_the_lowest_map(make: Make) -> None:
    """Frames a, b, a, b, a: maps 0, 2 and 4 are one map, as are 1 and 3, so
    every cell's maximum is a tie between at least two maps. The index must
    name the first of them, 0 or 1, never a later copy."""
    obj = make()
    n = _frame_len(obj)
    a, b = _frames(n, seed=13), _frames(n, seed=14)
    x = np.concatenate([a, b, a, b, a])
    p = _powers(obj.execute_all(x))
    assert np.array_equal(p[0], p[2]) and np.array_equal(p[1], p[3])
    power, index = obj.execute_power_max(x)
    want = np.where(p[1] > p[0], 1, 0).astype(np.uint32)
    np.testing.assert_array_equal(index, want)
    np.testing.assert_array_equal(index, np.argmax(p, axis=0))
    np.testing.assert_array_equal(power, p.max(axis=0))
    # Both earlier copies win somewhere, so both halves of the rule ran.
    assert set(np.unique(index)) == {0, 1}


# ── the cancellation case #2683 names ────────────────────────────────────


@pytest.mark.parametrize("make", MAKERS)
def test_a_statistic_of_each_map_is_not_the_statistic_of_their_max(
    make: Make,
) -> None:
    """Peak-to-mean is a statistic of ONE map. Map 0 is a clean, peaked
    correlation; map 1 is strong noise, flat and high. The pointwise maximum
    takes map 0's peak over map 1's floor, so its peak-to-mean is far below
    map 0's own: reducing first and taking the statistic after loses the
    detection a per-map statistic keeps. ``execute_peaks`` gives the
    per-map one, and must agree with NumPy over ``execute_all``'s maps."""
    obj = make()
    n = _frame_len(obj)
    clean = _clean(make)
    loud = (8.0 * _frames(n, seed=15)).astype(np.complex64)
    x = np.concatenate([clean, loud])
    p = _powers(obj.execute_all(x))

    peak, _row, _col, mean = obj.execute_peaks(x)
    per_map = peak / mean
    want = p.max(axis=1) / p.mean(axis=1, dtype=np.float64)
    np.testing.assert_allclose(per_map, want, rtol=1e-5)

    pm, _ = obj.execute_power_max(x)
    of_the_max = float(pm.max() / pm.mean(dtype=np.float64))
    best_map = float(per_map.max())
    assert best_map > 1.5 * of_the_max, (best_map, of_the_max)


# ── a batch mid-dwell, and reset() around one ────────────────────────────


def _run(obj: Correlator, method: str, x: np.ndarray):
    return getattr(obj, method)(x)


def _parts(result) -> tuple:
    """A reducer's arrays: power_max and peaks return a tuple of them."""
    return result if isinstance(result, tuple) else (result,)


#: the reducers beyond all, coherent and peaks, which test_correlator_batch
#: already holds mid-dwell
REDUCERS = ["execute_power_sum", "execute_power_max"]


@pytest.mark.parametrize("make", MAKERS)
@pytest.mark.parametrize("method", REDUCERS)
def test_a_reducer_mid_dwell_leaves_the_dwell_alone(
    make: Make, method: str
) -> None:
    """One frame into a dwell of three, then a batch of H frames and a
    batch of one (the split path execute() runs): count, the state blob and
    the next dump are what a twin that ran no batch has."""
    obj, twin = make(dwell=3), make(dwell=3)
    n = _frame_len(obj)
    x = _frames(n, seed=16)
    assert obj.execute(x) is None and twin.execute(x) is None
    blob = obj.get_state()
    for frames in (H, 1):
        _run(obj, method, _frames(frames * n, seed=17))
    assert obj.count == 1
    assert obj.get_state() == blob
    assert obj.execute(x) is None and twin.execute(x) is None
    np.testing.assert_array_equal(obj.execute(x), twin.execute(x))


@pytest.mark.parametrize("make", MAKERS)
@pytest.mark.parametrize(
    "method",
    [
        "execute_all",
        "execute_coherent",
        "execute_power_sum",
        "execute_power_max",
        "execute_peaks",
    ],
)
def test_reset_before_and_after_a_batch(make: Make, method: str) -> None:
    """A batch's answer does not depend on the dwell, so it is the same
    mid-dwell, after a reset() and on a fresh object; and reset() after a
    batch leaves exactly a fresh object, whose next dwell dumps what a
    fresh one dumps."""
    obj, fresh = make(dwell=2), make(dwell=2)
    n = _frame_len(obj)
    x = _frames(H * n, seed=18)
    want = _run(fresh, method, x)

    assert obj.execute(_frames(n, seed=19)) is None  # a dwell in progress
    mid = _run(obj, method, x)
    obj.reset()  # reset BEFORE a batch
    after_reset = _run(obj, method, x)
    for got in (mid, after_reset):
        gp, wp = _parts(got), _parts(want)
        # zip(strict=True) is 3.10+; the floor is 3.9, so check by hand.
        assert len(gp) == len(wp), f"{method}: {len(gp)} parts, want {len(wp)}"
        for g, w in zip(gp, wp):
            np.testing.assert_array_equal(g, w)

    # A dwell in progress again, a batch, THEN the reset: what the reset
    # clears is the frame, and the batch between must not hide it from it.
    assert obj.execute(_frames(n, seed=21)) is None
    _run(obj, method, x)
    obj.reset()  # reset AFTER a batch
    assert obj.count == 0
    assert obj.get_state() == fresh.get_state()
    y = _frames(n, seed=20)
    assert obj.execute(y) is None and fresh.execute(y) is None
    np.testing.assert_array_equal(obj.execute(y), fresh.execute(y))


# ── out= reused across calls, poisoned per run ───────────────────────────


@pytest.mark.parametrize("make", MAKERS)
@pytest.mark.parametrize(
    ("method", "dtype"),
    [
        ("execute_all", np.complex64),
        ("execute_coherent", np.complex64),
        ("execute_power_sum", np.float32),
    ],
)
def test_a_reused_out_is_rewritten_whole_every_call(
    make: Make, method: str, dtype: type
) -> None:
    """One buffer through four calls on four inputs. Before each, every
    cell is set to NaN, so a cell the call did not write still reads NaN
    and a cell it wrote from a previous input differs from a fresh call."""
    obj = make()
    n = _frame_len(obj)
    n_out = obj.execute_max_out()
    shape = (H, n_out) if method == "execute_all" else (n_out,)
    out = np.empty(shape, dtype)
    for seed in range(21, 25):
        x = _frames(H * n, seed=seed)
        out.fill(np.nan)
        got = _run_out(obj, method, x, out)
        assert np.shares_memory(got, out)
        assert not np.isnan(out.view(np.float32)).any(), "a cell unwritten"
        np.testing.assert_array_equal(out, getattr(make(), method)(x))


def _run_out(obj: Correlator, method: str, x: np.ndarray, out: np.ndarray):
    return getattr(obj, method)(x, out=out)
