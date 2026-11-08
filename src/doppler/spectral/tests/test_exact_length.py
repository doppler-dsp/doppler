"""``execute`` takes exactly one frame: any other length is refused (#1925).

Every fixed-size ``execute`` in ``doppler.spectral`` (FFT, FFT2D) used to hand
the kernel the caller's array and let it read ``n`` samples whatever the array
held. A 64-sample ``Corr``'s ``execute(np.ones(10))`` read 64 samples out of a
10-sample allocation (a heap over-read); ``execute(np.ones(65536))`` was
accepted and silently truncated, which is how the corr benchmark published 206
GSa/s for 1/1024 of the samples it claimed (#1918). The in-place variants
transformed a buffer whose tail was never written.

The C kernels now refuse (SIZE_MAX, nothing read, written or counted) and the
bindings raise ``ValueError``. The cases below pin every entry point, on the
default path and on the ``out=`` path, plus the two things a refusal must not
do: advance a dwell, and leak a reference to the caller's array.
``execute_ci16``/``ci8`` count int values, two to a complex sample, so an odd
count is refused by the binding before the kernel sees it (#1933). The matching
C tests (``test_{fft,fft2d,corr,corr2d}_core.c``) hand the kernel
exact-size heap arrays so ASan sees an over-read. The correlator's cases
(it was ``Corr``/``Corr2D``) moved with the class to
``correlation/tests/test_correlator_exact_length.py``.
"""

from __future__ import annotations

import sys

import numpy as np
import pytest

from doppler.spectral import FFT, FFT2D

C64, C128 = np.complex64, np.complex128
N = 64


def _fft():
    return FFT(N)


def _fft2d():
    return FFT2D(8, 8)


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


#: the interleaved-integer entry points, which count int values, two to a
#: complex sample
INT_CASES = [("execute_ci16", np.int16), ("execute_ci8", np.int8)]
INT_IDS = ["ci16", "ci8"]


@pytest.mark.parametrize("method, dt", INT_CASES, ids=INT_IDS)
@pytest.mark.parametrize(
    "length",
    [2 * N + 1, 2 * N - 1, 2 * N + 2],
    ids=["2n+1", "2n-1", "2n+2"],
)
def test_an_interleaved_length_is_refused_in_the_callers_unit(
    method, dt, length
):
    """#1933: halving the element count rounded ``2n+1`` down to a valid
    frame, so the stray last value was accepted and ignored.

    ``2n+1`` is the case that was silently accepted. ``2n-1`` was already
    refused, by the kernel's frame check, with a message that counted in
    complex samples (``FFT.n``) for an array of ints; it is odd, so the
    binding now refuses it first. ``2n+2`` is even, so the kernel still
    refuses it. All three must read the same message, in the caller's unit,
    naming the length that was passed.
    """
    msg = (
        rf"^{method} takes exactly one frame: 2 \* FFT\.n = {2 * N} "
        rf"interleaved I/Q values, got {length}$"
    )
    with pytest.raises(ValueError, match=msg):
        getattr(FFT(N), method)(np.ones(length, dt))


@pytest.mark.parametrize("method, dt", INT_CASES, ids=INT_IDS)
def test_an_odd_refusal_does_not_leak_the_callers_array(method, dt):
    """The odd-count refusal returns before the kernel, on its own path, so
    it owes its own release of the converted input."""
    obj = FFT(N)
    x = np.ones(2 * N + 1, dt)
    before = sys.getrefcount(x)
    for _ in range(200):
        with pytest.raises(ValueError):
            getattr(obj, method)(x)
    assert sys.getrefcount(x) == before
