"""DelayDoppler: the delay-Doppler map of a raw stream (#2666).

``DelayDoppler`` is the Corr2D view that takes the raw stream -- ``ny`` code
periods back to back -- and returns the centered ``ny_out x nx`` map that a
caller used to assemble from a zero-padded Corr2D reference plus a numpy
slow-time FFT. These tests hold it to that numpy pipeline, and to the
``out=`` contract a streaming caller runs it under.
"""

from __future__ import annotations

import tracemalloc
from typing import TYPE_CHECKING

import numpy as np
import pytest

from doppler.spectral import Corr2D, DelayDoppler

if TYPE_CHECKING:
    from numpy.typing import NDArray


def _code(nx: int, seed: int = 0) -> NDArray[np.complex64]:
    rng = np.random.default_rng(seed)
    return np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)


def _noise(n: int, seed: int) -> NDArray[np.complex64]:
    rng = np.random.default_rng(seed)
    x = rng.standard_normal(n) + 1j * rng.standard_normal(n)
    return x.astype(np.complex64)


def _numpy_map(
    frames: list[NDArray[np.complex64]],
    code: NDArray[np.complex64],
    ny: int,
    ny_out: int,
) -> NDArray[np.complex128]:
    """The numpy pipeline the object replaces, in double.

    Per row, ``ifft(fft(row) * conj(fft(code)))`` (numpy's ifft carries the
    1/nx), summed over the dwell's frames, then the ``ny_out``-point
    slow-time FFT down each column, unnormalized, centered by fftshift.
    """
    nx = code.size
    c = np.conj(np.fft.fft(code.astype(np.complex128)))
    acc = sum(
        np.fft.fft(f.reshape(ny, nx).astype(np.complex128), axis=1) * c
        for f in frames
    )
    r = np.fft.ifft(acc, axis=1)
    return np.fft.fftshift(np.fft.fft(r, n=ny_out, axis=0), axes=0)


@pytest.mark.parametrize(
    ("ny", "nx", "ny_out", "dwell", "nthreads"),
    [
        (8, 64, 0, 1, 1),
        (16, 128, 48, 2, 1),
        (7, 33, 9, 3, 4),
        (32, 256, 64, 1, 0),
        (1, 16, 4, 1, 2),
    ],
)
def test_matches_numpy(
    ny: int, nx: int, ny_out: int, dwell: int, nthreads: int
) -> None:
    code = _code(nx)
    frames = [_noise(ny * nx, seed=s) for s in range(dwell)]
    dd = DelayDoppler(
        code, ny=ny, dwell=dwell, nthreads=nthreads, ny_out=ny_out
    )
    for f in frames[:-1]:
        assert dd.execute(f) is None
    got = dd.execute(frames[-1])
    assert got is not None
    nyo = ny_out or ny
    want = _numpy_map(frames, code, ny, nyo)
    err = np.max(np.abs(got.reshape(nyo, nx) - want)) / np.max(np.abs(want))
    assert err < 1e-5
    assert dd.ny == ny and dd.nx == nx and dd.ny_out == nyo
    assert dd.doppler_step == 1.0 / (nyo * nx)


def test_known_delay_and_doppler_peak() -> None:
    """Delay 37, Doppler -5 native bins -> column 37, row ny/2 - 5."""
    ny, nx, d, k = 32, 256, 37, -5
    code = _code(nx, seed=3)
    n = np.arange(ny * nx)
    x = np.tile(np.roll(code, d), ny) * np.exp(2j * np.pi * k * n / (ny * nx))
    dd = DelayDoppler(code, ny=ny)
    m = dd.execute(x.astype(np.complex64)).reshape(ny, nx)
    row, col = np.unravel_index(np.argmax(np.abs(m)), m.shape)
    assert (int(row) - ny // 2, int(col)) == (k, d)


def test_matches_corr2d_plus_numpy_slow_time() -> None:
    """The view is Corr2D's map with the slow-time FFT moved into C."""
    ny, nx = 16, 64
    code = _code(nx, seed=9)
    x = _noise(ny * nx, seed=4)
    ref = np.zeros((ny, nx), np.complex64)
    ref[0] = code
    surf = Corr2D(ref=ref).execute(x).reshape(ny, nx)
    want = np.fft.fftshift(np.fft.fft(surf, axis=0), axes=0)
    got = DelayDoppler(code, ny=ny).execute(x).reshape(ny, nx)
    assert np.max(np.abs(got - want)) / np.max(np.abs(want)) < 1e-5


def test_bit_identical_across_threads() -> None:
    ny, nx, ny_out = 37, 100, 50
    code = _code(nx, seed=2)
    x = _noise(ny * nx, seed=5)
    maps = [
        DelayDoppler(code, ny=ny, nthreads=t, ny_out=ny_out).execute(x)
        for t in (1, 2, 7)
    ]
    assert maps[0].tobytes() == maps[1].tobytes() == maps[2].tobytes()


class TestOut:
    """``out=``: the configuration a streaming caller runs."""

    ny, nx, ny_out, dwell = 8, 64, 16, 2

    def _make(self) -> DelayDoppler:
        return DelayDoppler(
            _code(self.nx), ny=self.ny, dwell=self.dwell, ny_out=self.ny_out
        )

    def test_filled_in_place(self) -> None:
        dd = self._make()
        out = np.full(dd.execute_max_out(), 7 + 7j, np.complex64)
        assert out.size == self.ny_out * self.nx
        x = _noise(self.ny * self.nx, seed=1)
        assert dd.execute(x, out=out) is None  # mid-dwell: untouched
        assert np.all(out == 7 + 7j)
        m = dd.execute(x, out=out)
        # The return is a view of the caller's buffer, not a copy.
        assert m is not None and m.base is out
        assert np.shares_memory(m, out) and m.size == out.size

    def test_matches_no_out_path_bytewise(self) -> None:
        a, b = self._make(), self._make()
        out = np.empty(a.execute_max_out(), np.complex64)
        for s in range(2 * self.dwell):
            x = _noise(self.ny * self.nx, seed=s)
            ma, mb = a.execute(x, out=out), b.execute(x)
            assert (ma is None) == (mb is None)
            if mb is not None:
                assert out.tobytes() == mb.tobytes()

    def test_reused_with_no_new_allocation(self) -> None:
        dd = self._make()
        out = np.empty(dd.execute_max_out(), np.complex64)
        x = _noise(self.ny * self.nx, seed=6)
        dd.execute(x, out=out)  # warm: any lazy first-call allocation
        dd.execute(x, out=out)
        ptr = out.ctypes.data
        tracemalloc.start()
        try:
            for _ in range(8 * self.dwell):
                dd.execute(x, out=out)
            _, peak = tracemalloc.get_traced_memory()
        finally:
            tracemalloc.stop()
        # numpy reports its data allocations to tracemalloc: a fresh map
        # per dump would show as ny_out*nx*8 bytes. A view header is not.
        assert peak < out.nbytes // 4
        assert out.ctypes.data == ptr

    def test_too_small_refused(self) -> None:
        dd = self._make()
        out = np.empty(dd.execute_max_out() - 1, np.complex64)
        with pytest.raises(ValueError, match="out has"):
            dd.execute(_noise(self.ny * self.nx, seed=0), out=out)

    def test_wrong_dtype_refused(self) -> None:
        dd = self._make()
        out = np.empty(dd.execute_max_out(), np.complex128)
        with pytest.raises(TypeError, match="out must be"):
            dd.execute(_noise(self.ny * self.nx, seed=0), out=out)


def test_wrong_frame_length_refused() -> None:
    dd = DelayDoppler(_code(16), ny=4)
    with pytest.raises(ValueError, match="ny\\*nx"):
        dd.execute(np.zeros(63, np.complex64))
    assert dd.count == 0


@pytest.mark.parametrize(
    "kwargs",
    [
        {"code": np.zeros(0, np.complex64), "ny": 4},
        {"code": _code(16), "ny": 0},
        {"code": _code(16), "ny": 4, "dwell": 0},
        {"code": _code(16), "ny": 4, "ny_out": 3},
    ],
)
def test_refusals(kwargs: dict[str, object]) -> None:
    with pytest.raises(ValueError, match="DelayDoppler: invalid parameter"):
        DelayDoppler(**kwargs)  # type: ignore[arg-type]


def test_state_refuses_the_other_flavor_and_ny_out() -> None:
    ny, nx = 4, 8
    code = _code(nx)
    ref = np.zeros((ny, nx), np.complex64)
    ref[0] = code
    dd = DelayDoppler(code, ny=ny, dwell=3, ny_out=8)
    dd.execute(_noise(ny * nx, seed=1))
    blob = dd.get_state()
    corr = Corr2D(ref=ref, dwell=3)
    with pytest.raises(ValueError):
        corr.set_state(blob)
    with pytest.raises(ValueError):
        dd.set_state(corr.get_state())
    other = DelayDoppler(code, ny=ny, dwell=3, ny_out=6)
    assert other.state_bytes() == dd.state_bytes()
    with pytest.raises(ValueError):
        other.set_state(blob)
    assert other.count == 0
