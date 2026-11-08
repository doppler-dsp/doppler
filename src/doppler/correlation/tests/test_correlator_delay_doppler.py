"""The delay-Doppler flavor: the map of a raw stream (#2666).

``Correlator(code, doppler_rows=ny)`` (was ``DelayDoppler``) is the 2-D
correlator that takes the raw stream -- ``ny`` code
periods back to back -- and returns the centered ``ny_out x nx`` map that a
caller used to assemble from a zero-padded 2-D reference plus a numpy
slow-time FFT. These tests hold it to that numpy pipeline, and to the
``out=`` contract a streaming caller runs it under.
"""

from __future__ import annotations

import tracemalloc
from typing import TYPE_CHECKING

import numpy as np
import pytest

from doppler.correlation import Correlator

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
    dd = Correlator(
        code, doppler_rows=ny, dwell=dwell, nthreads=nthreads, ny_out=ny_out
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
    dd = Correlator(code, doppler_rows=ny)
    m = dd.execute(x.astype(np.complex64)).reshape(ny, nx)
    row, col = np.unravel_index(np.argmax(np.abs(m)), m.shape)
    assert (int(row) - ny // 2, int(col)) == (k, d)


def test_matches_corr2d_plus_numpy_slow_time() -> None:
    """The delay-Doppler flavor is the 2-D map with the slow-time FFT moved
    into C."""
    ny, nx = 16, 64
    code = _code(nx, seed=9)
    x = _noise(ny * nx, seed=4)
    ref = np.zeros((ny, nx), np.complex64)
    ref[0] = code
    surf = Correlator(ref=ref).execute(x).reshape(ny, nx)
    want = np.fft.fftshift(np.fft.fft(surf, axis=0), axes=0)
    got = Correlator(code, doppler_rows=ny).execute(x).reshape(ny, nx)
    assert np.max(np.abs(got - want)) / np.max(np.abs(want)) < 1e-5


def test_bit_identical_across_threads() -> None:
    ny, nx, ny_out = 37, 100, 50
    code = _code(nx, seed=2)
    x = _noise(ny * nx, seed=5)
    maps = [
        Correlator(code, doppler_rows=ny, nthreads=t, ny_out=ny_out).execute(x)
        for t in (1, 2, 7)
    ]
    assert maps[0].tobytes() == maps[1].tobytes() == maps[2].tobytes()


class TestOut:
    """``out=``: the configuration a streaming caller runs."""

    ny, nx, ny_out, dwell = 8, 64, 16, 2

    def _make(self) -> Correlator:
        return Correlator(
            _code(self.nx),
            doppler_rows=self.ny,
            dwell=self.dwell,
            ny_out=self.ny_out,
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
    dd = Correlator(_code(16), doppler_rows=4)
    with pytest.raises(ValueError, match="ny\\*nx"):
        dd.execute(np.zeros(63, np.complex64))
    assert dd.count == 0


@pytest.mark.parametrize(
    ("kwargs", "match"),
    [
        (
            {"ref": np.zeros(0, np.complex64), "doppler_rows": 4},
            "Correlator: ref is empty",
        ),
        (
            {"ref": _code(16), "doppler_rows": 4, "dwell": 0},
            "Correlator: dwell must be at least 1",
        ),
        (
            {"ref": _code(16), "doppler_rows": 4, "ny_out": 3},
            "ny_out pads the Doppler rows",
        ),
    ],
)
def test_refusals(kwargs: dict[str, object], match: str) -> None:
    # DelayDoppler's refusal of ny=0 has no spelling here: doppler_rows=0
    # is the class's "no map", which builds the 1-D flavor (ruled #2752 4c).
    with pytest.raises(ValueError, match=match):
        Correlator(**kwargs)  # type: ignore[arg-type]


def test_state_refuses_the_other_flavor_and_ny_out() -> None:
    ny, nx = 4, 8
    code = _code(nx)
    ref = np.zeros((ny, nx), np.complex64)
    ref[0] = code
    dd = Correlator(code, doppler_rows=ny, dwell=3, ny_out=8)
    dd.execute(_noise(ny * nx, seed=1))
    blob = dd.get_state()
    corr = Correlator(ref=ref, dwell=3)
    with pytest.raises(ValueError):
        corr.set_state(blob)
    with pytest.raises(ValueError):
        dd.set_state(corr.get_state())
    other = Correlator(code, doppler_rows=ny, dwell=3, ny_out=6)
    assert other.state_bytes() == dd.state_bytes()
    with pytest.raises(ValueError):
        other.set_state(blob)
    assert other.count == 0
