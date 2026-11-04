"""Corr2D and DelayDoppler batches through the binding (#2683).

The C tests (``test_corr2d_core.c``) hold the arithmetic: map h equals a
fresh dwell-1 object's, every reduction folds in h order, every refusal
writes nothing. These hold what only the binding can get wrong: the shapes
and dtypes each method returns, that ``out=`` is filled in place and handed
back as a view, that a refusal is a ``ValueError``, and that a batch leaves
the dwell where it was, on both classes.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.spectral import Corr2D, DelayDoppler

NY, NX, H = 4, 16, 5


def _code() -> np.ndarray:
    rng = np.random.default_rng(2683)
    return np.exp(2j * np.pi * rng.random(NX)).astype(np.complex64)


def _frames(n: int, seed: int = 1) -> np.ndarray:
    rng = np.random.default_rng(seed)
    z = rng.standard_normal(n) + 1j * rng.standard_normal(n)
    return z.astype(np.complex64)


def _corr2d() -> Corr2D:
    ref = np.zeros((NY, NX), np.complex64)
    ref[0] = _code()
    return Corr2D(ref=ref, nthreads=2)


def _dd() -> DelayDoppler:
    return DelayDoppler(_code(), ny=NY, ny_out=2 * NY, nthreads=2)


MAKERS = [pytest.param(_corr2d, id="Corr2D"), pytest.param(_dd, id="DD")]


def _fresh_maps(make, x: np.ndarray) -> np.ndarray:
    """Each frame through its own fresh dwell-1 object's execute()."""
    obj = make()
    n = obj.ny * obj.nx
    maps = []
    for h in range(len(x) // n):
        maps.append(make().execute(x[h * n : (h + 1) * n]))
    return np.stack(maps)


@pytest.mark.parametrize("make", MAKERS)
def test_every_method_returns_its_shape_and_dtype(make) -> None:
    obj = make()
    x = _frames(H * obj.ny * obj.nx)
    n_out = obj.n_out

    maps = obj.execute_all(x)
    assert maps.shape == (H, n_out) and maps.dtype == np.complex64
    np.testing.assert_array_equal(maps, _fresh_maps(make, x))

    coh = obj.execute_coherent(x)
    assert coh.shape == (n_out,) and coh.dtype == np.complex64

    ps = obj.execute_power_sum(x)
    assert ps.shape == (n_out,) and ps.dtype == np.float32

    pm, idx = obj.execute_power_max(x)
    assert pm.shape == idx.shape == (n_out,)
    assert pm.dtype == np.float32 and idx.dtype == np.uint32
    np.testing.assert_array_equal(idx, np.argmax(np.abs(maps) ** 2, axis=0))

    peak, row, col, mean = obj.execute_peaks(x)
    assert peak.shape == row.shape == col.shape == mean.shape == (H,)
    assert row.dtype == col.dtype == np.uint32
    flat = np.argmax(np.abs(maps) ** 2, axis=1)
    # DelayDoppler's delay axis is native, so it has no nx_out property.
    width = getattr(obj, "nx_out", obj.nx)
    np.testing.assert_array_equal(row, flat // width)
    np.testing.assert_array_equal(col, flat % width)
    np.testing.assert_allclose(
        mean, np.mean(np.abs(maps) ** 2, axis=1), rtol=1e-5
    )


@pytest.mark.parametrize("make", MAKERS)
def test_out_is_filled_in_place_and_overwritten_by_the_next_call(
    make,
) -> None:
    obj = make()
    n = obj.ny * obj.nx
    out = np.empty((H, obj.n_out), np.complex64)
    first = obj.execute_all(_frames(H * n, seed=1), out=out)
    assert np.shares_memory(first, out)
    kept = first.copy()
    obj.execute_all(_frames(H * n, seed=2), out=out)
    # The view is of the caller's buffer, so the next call overwrote it.
    assert not np.array_equal(first, kept)
    np.testing.assert_array_equal(first, out)


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
def test_a_ragged_or_empty_batch_is_a_value_error(make, method) -> None:
    obj = make()
    n = obj.ny * obj.nx
    for length in (0, 1, n - 1, n + 1, 2 * n + 1):
        with pytest.raises(ValueError):
            getattr(obj, method)(_frames(length))


@pytest.mark.parametrize("make", MAKERS)
def test_a_short_out_is_a_value_error(make) -> None:
    obj = make()
    x = _frames(H * obj.ny * obj.nx)
    with pytest.raises(ValueError):
        obj.execute_all(x, out=np.empty((H - 1, obj.n_out), np.complex64))
    with pytest.raises(ValueError):
        obj.execute_coherent(x, out=np.empty(obj.n_out - 1, np.complex64))
    with pytest.raises(ValueError):
        obj.execute_power_sum(x, out=np.empty(obj.n_out - 1, np.float32))


@pytest.mark.parametrize("make", MAKERS)
@pytest.mark.parametrize(
    ("method", "dtype"),
    [("execute_coherent", np.complex64), ("execute_power_sum", np.float32)],
)
def test_a_one_map_out_needs_one_map_not_the_input(
    make, method, dtype
) -> None:
    # The result is ONE map whatever the batch, so `out=` of exactly that
    # (what *_max_out() reports) is enough: the binding used to ask for as
    # many elements as the input had (#2818), which refused it.
    obj = make()
    x = _frames(H * obj.ny * obj.nx)
    n_out = getattr(obj, method + "_max_out")()
    assert n_out == obj.n_out and n_out < len(x)
    out = np.empty(n_out, dtype)
    got = getattr(obj, method)(x, out=out)
    assert got.base is out or got is out
    np.testing.assert_array_equal(got, getattr(obj, method)(x))


@pytest.mark.parametrize("make", MAKERS)
def test_a_batch_mid_dwell_leaves_the_dwell_alone(make) -> None:
    # A dwell of 2, so one frame leaves a dwell in progress.
    cls = Corr2D if make is _corr2d else DelayDoppler
    obj, twin = cls(**_kwargs(make, 2)), cls(**_kwargs(make, 2))
    n = obj.ny * obj.nx
    x = _frames(n, seed=5)
    assert obj.execute(x) is None and twin.execute(x) is None
    blob = obj.get_state()
    # H frames take the whole-map path, one frame the split path that
    # execute() also runs: both must leave the dwell alone.
    for frames in (H, 1):
        for method in ("execute_all", "execute_coherent", "execute_peaks"):
            getattr(obj, method)(_frames(frames * n, seed=6))
    assert obj.count == 1
    assert obj.get_state() == blob
    np.testing.assert_array_equal(obj.execute(x), twin.execute(x))


def _kwargs(make, dwell: int) -> dict:
    if make is _corr2d:
        ref = np.zeros((NY, NX), np.complex64)
        ref[0] = _code()
        return {"ref": ref, "dwell": dwell, "nthreads": 2}
    return {
        "code": _code(),
        "ny": NY,
        "ny_out": 2 * NY,
        "dwell": dwell,
        "nthreads": 2,
    }


def test_execute_out_needs_one_dump_not_the_frame() -> None:
    # A col_out object dumps ny values from an ny * nx frame: `out=` of the
    # dump is enough. The binding used to ask for the frame's length (#2818).
    ny, nx = 4, 8
    ref = np.zeros((ny, nx), np.complex64)
    ref[0, 0] = 1.0
    obj = Corr2D(ref=ref, dwell=1, nthreads=1, col_out=0)
    x = np.ones((ny, nx), np.complex64)
    assert obj.execute_max_out() == ny < ny * nx
    out = np.empty(ny, np.complex64)
    got = obj.execute(x, out=out)
    assert got.base is out
    np.testing.assert_array_equal(
        got, Corr2D(ref=ref, dwell=1, nthreads=1, col_out=0).execute(x)
    )
    with pytest.raises(ValueError):
        obj.execute(x, out=np.empty(ny - 1, np.complex64))
