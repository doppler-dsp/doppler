"""``execute`` takes exactly one frame: any other length is refused (#1925).

The correlator's half of ``spectral/tests/test_exact_length.py``, moved with
the class (#2752 step 4c; it was ``Corr`` and ``Corr2D``). The 1-D map once
read its whole reference length out of whatever array the caller passed: a
10-sample array was a heap over-read and a 65536-sample one was silently
truncated, which is how the corr benchmark published 206 GSa/s for 1/1024 of
the samples it claimed (#1918).

The C kernel now refuses (SIZE_MAX, nothing read, written or counted) and the
binding raises ``ValueError``. The cases below pin both map flavors, on the
default path and on the ``out=`` path, plus the two things a refusal must not
do: advance a dwell, and leak a reference to the caller's array. The matching
C tests hand the kernel exact-size heap arrays so ASan sees an over-read.
"""

from __future__ import annotations

import sys

import numpy as np
import pytest

from doppler.correlation import Correlator

C64 = np.complex64
N = 64


def _corr():
    """The 1-D map flavor (was ``Corr``, dwell 1)."""
    return Correlator(np.ones(N, C64), dwell=1)


def _corr2d():
    """The 2-D map flavor (was ``Corr2D``, dwell 1)."""
    return Correlator(np.ones((8, 8), C64), dwell=1)


#: (id, make, method, dtype, input elements per frame)
CASES = [
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
        "two-elements": 2,
        "half-a-frame": per_frame // 2,
        "two-frames": 2 * per_frame,
    }[shape]
    with pytest.raises(ValueError, match="exactly one frame"):
        getattr(make(), method)(np.ones(length, dt))


@pytest.mark.parametrize("case", CASES, ids=IDS)
def test_a_wrong_length_is_refused_on_the_out_path_too(case):
    _, make, method, dt, per_frame = case
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
    obj = Correlator(ref, dwell=2)
    good = np.ones(N, C64)
    assert obj.execute(good) is None
    for bad in (np.ones(3, C64), np.ones(2 * N, C64), np.ones(0, C64)):
        with pytest.raises(ValueError):
            obj.execute(bad)
        assert obj.count == 1
    assert obj.execute(good) is not None
