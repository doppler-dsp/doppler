"""``execute`` takes exactly one frame: any other length is refused (#1925).

Every fixed-size ``execute`` in ``doppler.spectral`` (FFT, FFT2D, Corr, Corr2D)
used to hand the kernel the caller's array and let it read ``n`` samples
whatever the array held. ``Corr(ref64).execute(np.ones(10))`` read 64 samples
out of a 10-sample allocation (a heap over-read); ``execute(np.ones(65536))``
was accepted and silently truncated, which is how the corr benchmark
published 206 GSa/s for 1/1024 of the samples it claimed (#1918). The in-place
variants transformed a buffer whose tail was never written.

The C kernels now refuse (SIZE_MAX, nothing read, written or counted) and the
bindings raise ``ValueError``. The cases below pin every entry point, on the
default path and on the ``out=`` path, plus the two things a refusal must not
do: advance a dwell, and leak a reference to the caller's array. The matching
C tests (``test_{fft,fft2d,corr,corr2d}_core.c``) hand the kernel
exact-size heap arrays so ASan sees an over-read.
"""

from __future__ import annotations

import sys

import numpy as np
import pytest

from doppler.spectral import FFT, FFT2D, Corr, Corr2D

C64, C128 = np.complex64, np.complex128
N = 64


def _fft():
    return FFT(N)


def _fft2d():
    return FFT2D(8, 8)


def _corr():
    return Corr(np.ones(N, C64), 1)


def _corr2d():
    return Corr2D(np.ones((8, 8), C64), 1)


#: (id, make, method, dtype, input elements per frame)
CASES = [
    ("fft.cf32", _fft, "execute_cf32", C64, N),
    ("fft.cf64", _fft, "execute_cf64", C128, N),
    ("fft.inplace_cf32", _fft, "execute_inplace_cf32", C64, N),
    ("fft.inplace_cf64", _fft, "execute_inplace_cf64", C128, N),
    ("fft.ci16", _fft, "execute_ci16", np.int16, 2 * N),
    ("fft.ci8", _fft, "execute_ci8", np.int8, 2 * N),
    ("fft2d.cf32", _fft2d, "execute_cf32", C64, N),
    ("fft2d.cf64", _fft2d, "execute_cf64", C128, N),
    ("fft2d.inplace_cf32", _fft2d, "execute_inplace_cf32", C64, N),
    ("fft2d.inplace_cf64", _fft2d, "execute_inplace_cf64", C128, N),
    ("corr", _corr, "execute", C64, N),
    ("corr2d", _corr2d, "execute", C64, N),
]
IDS = [c[0] for c in CASES]


@pytest.mark.parametrize("case", CASES, ids=IDS)
@pytest.mark.parametrize(
    "shape", ["empty", "two-elements", "half-a-frame", "two-frames"]
)
def test_a_wrong_length_is_refused(case, shape):
    _, make, method, dt, per_frame = case
    length = {
        "empty": 0,
        # 2, not 1: ci16/ci8 count int values, two to a complex sample
        "two-elements": 2,
        "half-a-frame": per_frame // 2,
        "two-frames": 2 * per_frame,
    }[shape]
    with pytest.raises(ValueError, match="exactly one frame"):
        getattr(make(), method)(np.ones(length, dt))


@pytest.mark.parametrize("case", CASES, ids=IDS)
def test_a_wrong_length_is_refused_on_the_out_path_too(case):
    _, make, method, dt, per_frame = case
    if per_frame == 2 * N:
        pytest.skip("execute_ci16/ci8 take no out=")
    obj = make()
    out = np.full(N, -7, dt)
    with pytest.raises(ValueError, match="exactly one frame"):
        getattr(obj, method)(np.ones(per_frame // 2, dt), out)
    assert np.all(out == -7), "a refusal wrote into the caller's out"


@pytest.mark.parametrize("case", CASES, ids=IDS)
def test_the_right_length_still_works_after_a_refusal(case):
    _, make, method, dt, per_frame = case
    obj = make()
    with pytest.raises(ValueError):
        getattr(obj, method)(np.ones(per_frame // 2, dt))
    r = getattr(obj, method)(np.ones(per_frame, dt))
    assert r is not None and len(r) == N


@pytest.mark.parametrize("case", CASES, ids=IDS)
def test_a_refusal_does_not_leak_the_callers_array(case):
    _, make, method, dt, per_frame = case
    obj = make()
    x = np.ones(per_frame // 2, dt)
    before = sys.getrefcount(x)
    for _ in range(200):
        with pytest.raises(ValueError):
            getattr(obj, method)(x)
    assert sys.getrefcount(x) == before


@pytest.mark.parametrize("make", [_corr, _corr2d], ids=["corr", "corr2d"])
def test_a_refusal_is_not_counted_toward_the_dwell(make):
    """dwell 2: refused calls between two good frames must not dump early or
    late; the dump is still on the second good frame."""
    ref = np.ones(N, C64) if make is _corr else np.ones((8, 8), C64)
    obj = (Corr if make is _corr else Corr2D)(ref, 2)
    good = np.ones(N, C64)
    assert obj.execute(good) is None
    for bad in (np.ones(3, C64), np.ones(2 * N, C64), np.ones(0, C64)):
        with pytest.raises(ValueError):
            obj.execute(bad)
        assert obj.count == 1
    assert obj.execute(good) is not None
