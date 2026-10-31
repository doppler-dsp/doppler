"""The hypothesis bank through the binding (#2683 phase 3).

The C tests (``test_corr2d_core.c``, ``_test_bank`` and
``_test_bank_detect``) pin every claim of the bank against hand-corrected
frames. These check what the binding adds: the shapes and dtypes each method
returns, that a bank is the frame batch of corrected copies through the same
binding, that a refusal is a ``ValueError``, and that the dwell is untouched.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.spectral import Corr2D, DelayDoppler

NY, NX = 8, 32
NO_RATES = np.empty(0, np.complex64)


def _code(seed: int = 7) -> np.ndarray:
    rng = np.random.default_rng(seed)
    return rng.choice(np.array([-1.0, 1.0]), NX).astype(np.complex64)


def _signal(code: np.ndarray, carrier_bins: float, delay: int) -> np.ndarray:
    m = np.arange(NY * NX)
    return (
        np.tile(np.roll(code, delay), NY)
        * np.exp(2j * np.pi * carrier_bins / NX * m)
    ).astype(np.complex64)


def _corrected(x, g: int, L: int, w=None) -> np.ndarray:
    """The frame corrected by hand: within each period, and by a rate."""
    n = np.arange(NX)
    mix = np.tile(np.exp(-2j * np.pi * g * n / (L * NX)), NY)
    y = x * mix
    if w is not None:
        y = y * np.repeat(w, NX)
    return y.astype(np.complex64)


def test_bank_power_max_is_the_corrected_batch() -> None:
    """A bank equals the frame batch of hand-corrected copies."""
    rng = np.random.default_rng(3)
    code = _code()
    x = (
        rng.standard_normal(NY * NX) + 1j * rng.standard_normal(NY * NX)
    ).astype(np.complex64)
    rates = np.exp(1j * rng.uniform(0, 2 * np.pi, (2, NY))).astype(
        np.complex64
    )
    dd = DelayDoppler(code, ny=NY, ny_out=2 * NY, nthreads=2)
    pw, h = dd.bank_power_max(x, 3, -5, 4, rates.ravel())
    assert pw.shape == h.shape == (dd.n_out,)
    assert pw.dtype == np.float32 and h.dtype == np.uint32
    copies = np.concatenate(
        [_corrected(x, g, 3, w) for g in range(-5, 5) for w in rates]
    )
    bp, bi = dd.execute_power_max(copies)
    assert np.max(np.abs(pw - bp)) <= 1e-5 * bp.max()
    assert np.array_equal(h, bi)


def test_bank_detect_finds_the_fine_carrier() -> None:
    """Between two coarse points, the refine names the fine carrier."""
    code = _code()
    x = _signal(code, 1.25, 11)
    dd = DelayDoppler(code, ny=NY)
    coarse, _ = dd.bank_power_max(x, 2, -4, 4, NO_RATES)
    t = 0.5 * float(coarse.max())
    power, g, rate, _row, col = dd.bank_detect(x, 2, 4, -4, 4, t, t, NO_RATES)
    assert (power.dtype, g.dtype, rate.dtype) == (
        np.float32,
        np.int32,
        np.uint32,
    )
    assert len(power) == 1
    assert (int(g[0]), int(rate[0]), int(col[0])) == (5, 0, 11)
    assert power[0] > coarse.max()  # the straddle recovered


@pytest.mark.parametrize("general", [False, True])
def test_bank_runs_on_corr2d(general: bool) -> None:
    """Every Corr2D path runs a bank, rates included: the corrected batch."""
    code = _code()
    ref = np.zeros((NY, NX), np.complex64)
    ref[0] = code
    if general:  # energy off row 0: the general 2-D path
        ref[1:] = 0.5 * code
    c = Corr2D(ref=ref, dwell=1, nthreads=1)
    w = np.linspace(0.5, 1.5, 2 * NY).astype(np.complex64).reshape(2, NY)
    x = _signal(code, 0.7, 3)
    pw, h = c.bank_power_max(x, 2, -1, 1, w.ravel())
    copies = np.concatenate(
        [_corrected(x, g, 2, r) for g in range(-1, 2) for r in w]
    )
    bp, bi = c.execute_power_max(copies)
    assert np.max(np.abs(pw - bp)) <= 1e-5 * bp.max()
    assert np.array_equal(h, bi)


@pytest.mark.parametrize(
    "args",
    [
        (2, 1, 0),  # g_min > g_max
        (0, -1, 1),  # L == 0
    ],
)
def test_a_bad_grid_is_a_value_error(args) -> None:
    code = _code()
    dd = DelayDoppler(code, ny=NY)
    with pytest.raises(ValueError):
        dd.bank_power_max(_signal(code, 0.0, 1), *args, NO_RATES)


def test_a_wrong_frame_or_rates_is_a_value_error() -> None:
    code = _code()
    dd = DelayDoppler(code, ny=NY)
    x = _signal(code, 0.0, 1)
    with pytest.raises(ValueError):
        dd.bank_power_max(x[:-1], 2, -1, 1, NO_RATES)
    with pytest.raises(ValueError):
        dd.bank_power_max(x, 2, -1, 1, np.ones(NY + 1, np.complex64))
    with pytest.raises(ValueError):
        dd.bank_detect(x, 3, 4, -1, 1, 0.0, 0.0, NO_RATES)  # 3 ∤ 4


def test_a_bank_leaves_the_dwell_alone() -> None:
    code = _code()
    dd = DelayDoppler(code, ny=NY, dwell=3)
    x = _signal(code, 0.5, 4)
    assert dd.execute(x) is None  # one frame into a dwell of three
    before = dd.get_state()
    dd.bank_power_max(x, 4, -4, 4, NO_RATES)
    dd.bank_detect(x, 2, 4, -2, 2, 0.0, 0.0, NO_RATES)
    assert dd.count == 1 and dd.get_state() == before
