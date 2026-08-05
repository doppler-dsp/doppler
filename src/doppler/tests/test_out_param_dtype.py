"""Uniform ``out=`` dtype contract across every type that accepts one.

An ``out=`` buffer exists so the caller controls the allocation: a streaming
loop hands the same array back on every call and reads its contents afterwards.
That contract has exactly one dangerous failure mode — accepting a buffer of
the wrong dtype and *casting* it. A ``PyArray_FROM_OTF`` cast writes into a
temporary, so the kernel fills the temporary, the temporary is discarded, and
the call still returns a correct-looking result. The caller's buffer, the whole
reason ``out=`` was passed, is never touched. Nothing raises, and a caller that
only reads the return value never finds out.

doppler carried that bug library-wide until jm 0.33.13 (doppler-filed, jm
gh-581); the handle generator had always rejected a mismatched buffer, so the
two generators disagreed about the same contract. This matrix is the gate that
keeps them agreeing: the guard lives in per-object CPython glue — some
jm-generated, some hand-owned in a sacred ``_ext_<obj>.c`` fragment — so it is
re-emitted, or re-hand-written, every time a fragment is regenerated. A
per-module test would leave most of that surface uncovered.

Asserted for every entry:

1. **A wrong-dtype ``out=`` raises** rather than silently casting to a temp.
2. **A correct-dtype ``out=`` is actually written**, so the guard cannot be
   "passed" by an implementation that rejects everything, or that ignores
   ``out=`` and returns a fresh array instead.

Examples
--------
>>> import numpy as np
>>> from doppler.filter import FIR
>>> f = FIR(np.array([1, 0, 0], dtype=np.complex64))
>>> buf = np.empty(64, dtype=np.float32)  # wrong dtype for out=
>>> f.execute(np.ones(64, dtype=np.complex64), out=buf)
Traceback (most recent call last):
    ...
TypeError: out must be a writable ndarray of the output dtype
"""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

import numpy as np
import pytest
from numpy.typing import NDArray

from doppler.agc import AGC
from doppler.cvt import F32ToI16, I16ToF32
from doppler.ddc import DDC, Ddcr
from doppler.delay import DelayCf64
from doppler.filter import FIR
from doppler.resample import CIC, Farrow, Resampler
from doppler.source import LO, NCO
from doppler.spectral import FFT
from doppler.wfm import PN

N = 64

_CF32 = np.ones(N, dtype=np.complex64)
_F32 = np.ones(N, dtype=np.float32)
_I16 = np.ones(N, dtype=np.int16)
_TAPS = np.array([0.5, 0.25, 0.25], dtype=np.complex64)

_Invoke = Callable[[Any, NDArray[Any]], Any]

# (name, make, invoke, out_dtype, max_out_attr, floor)
#
# `make()` builds a fresh instance and `invoke(obj, out)` calls the
# out=-accepting method with that buffer. The required buffer length is
# `max(getattr(obj, max_out_attr)(), floor)`: every object publishes a
# `<method>_max_out()` companion, but it is state-dependent and reads 0 before
# the first call, so `floor` carries the block length the call itself needs.
# DelayCf64.ptr must state its count explicitly. `ptr_max_out(n)` is a
# per-call bound — `min(n, num_taps)` — so sizing with floor=0 asks "how much
# for a zero-length request?", gets 0, and then invokes with ptr's own default
# (0 = all available = num_taps). The two halves have to name the same count,
# so the case passes 4 to both. Before gh-761 gave max_out its argument this
# was hidden: the no-arg form returned num_taps regardless of what was asked.
_CASES: list[tuple[str, Callable[[], Any], _Invoke, Any, str, int]] = [
    (
        "FIR.execute",
        lambda: FIR(_TAPS),
        lambda o, out: o.execute(_CF32, out=out),
        np.complex64,
        "execute_max_out",
        N,
    ),
    (
        "CIC.decimate",
        lambda: CIC(4),
        lambda o, out: o.decimate(_CF32, out=out),
        np.complex64,
        "decimate_max_out",
        N,
    ),
    (
        "DDC.execute",
        lambda: DDC(0.0, 0.25),
        lambda o, out: o.execute(_CF32, out=out),
        np.complex64,
        "execute_max_out",
        N,
    ),
    (
        "Ddcr.execute",
        lambda: Ddcr(0.0, 0.25),
        lambda o, out: o.execute(_F32, out=out),
        np.complex64,
        "execute_max_out",
        N,
    ),
    (
        "AGC.steps",
        lambda: AGC(),
        lambda o, out: o.steps(_CF32, out=out),
        np.complex64,
        "",
        N,
    ),
    (
        "DelayCf64.ptr",
        lambda: DelayCf64(4),
        lambda o, out: o.ptr(4, out=out),
        np.complex128,
        "ptr_max_out",
        4,
    ),
    (
        "FFT.execute_cf32",
        lambda: FFT(N),
        lambda o, out: o.execute_cf32(_CF32, out=out),
        np.complex64,
        "execute_cf32_max_out",
        N,
    ),
    (
        "LO.steps",
        lambda: LO(0.1),
        lambda o, out: o.steps(N, out=out),
        np.complex64,
        "steps_max_out",
        N,
    ),
    (
        "NCO.steps_u32_ctrl",
        lambda: NCO(0.1),
        lambda o, out: o.steps_u32_ctrl(_F32, out=out),
        np.uint32,
        "steps_u32_ctrl_max_out",
        N,
    ),
    (
        "PN.generate",
        lambda: PN(seed=1, length=7),
        lambda o, out: o.generate(out=out),
        np.uint8,
        "generate_max_out",
        N,
    ),
    (
        "F32ToI16.steps",
        lambda: F32ToI16(1.0),
        lambda o, out: o.steps(_F32, out=out),
        np.int16,
        "",
        N,
    ),
    (
        "I16ToF32.steps",
        lambda: I16ToF32(1.0),
        lambda o, out: o.steps(_I16, out=out),
        np.float32,
        "",
        N,
    ),
    (
        "Resampler.execute",
        lambda: Resampler(0.5),
        lambda o, out: o.execute(_CF32, out=out),
        np.complex64,
        "execute_max_out",
        N,
    ),
    (
        "Farrow.delay",
        lambda: Farrow(),
        lambda o, out: o.delay(_CF32, 0.5, out=out),
        np.complex64,
        "delay_max_out",
        N,
    ),
]

_IDS = [c[0] for c in _CASES]
_PARAMS = ("name", "make", "invoke", "dt", "max_out_attr", "floor")


def _out_len(obj: Any, max_out_attr: str, floor: int) -> int:
    """Length the binding demands: its published cap, or the block length."""
    if not max_out_attr:
        cap = 0
    else:
        fn = getattr(obj, max_out_attr)
        # gh-607: a per-call max_out() takes the call's count; older ones
        # take none. Ask for a block of `floor`, falling back to the no-arg
        # form for objects whose max_out did not gain a count.
        try:
            cap = fn(floor)
        except TypeError:
            cap = fn()
    return max(int(cap), floor)


# Candidate wrong dtypes, narrowest first. The right one to test with is
# whichever SAFELY casts to the target — see _wrong_dtype.
_CANDIDATES = (
    np.bool_,
    np.uint8,
    np.int8,
    np.uint16,
    np.int16,
    np.float16,
    np.float32,
    np.complex64,
)


def _wrong_dtype(want: Any) -> Any:
    """A wrong dtype that ``want`` can be *safely cast from*.

    This choice is the whole point of the matrix, and getting it wrong makes
    the test vacuous. An unsafe pair (float64 -> complex64) is refused by
    ``PyArray_FROM_OTF`` itself with "Cannot cast array data ... according to
    the rule 'safe'", so the call raises whether or not the dtype guard is
    present — a test using one passes even with the guard deleted, which is
    exactly what a sabotage check of this file caught.

    The silent, dangerous case is a *safe* cast (float32 -> complex64): numpy
    performs it happily into a temporary, and only the guard stops the caller's
    buffer being silently skipped. So pick a dtype that can_cast safely.
    """
    for cand in _CANDIDATES:
        if np.dtype(cand) != np.dtype(want) and np.can_cast(
            cand, want, "safe"
        ):
            return cand
    raise AssertionError(f"no safely-castable wrong dtype for {want!r}")


@pytest.mark.parametrize(_PARAMS, _CASES, ids=_IDS)
def test_wrong_out_dtype_raises(
    name: str,
    make: Callable[[], Any],
    invoke: _Invoke,
    dt: Any,
    max_out_attr: str,
    floor: int,
) -> None:
    """A mismatched out= buffer is refused, never cast into a temporary."""
    obj = make()
    n = _out_len(obj, max_out_attr, floor)
    wrong = _wrong_dtype(dt)
    # Guards the matrix against going vacuous: an unsafe pair would raise from
    # numpy's own cast rule and prove nothing about the binding's dtype check.
    assert np.can_cast(wrong, dt, "safe")
    bad = np.zeros(n, dtype=wrong)
    with pytest.raises((TypeError, ValueError)):
        invoke(obj, bad)


@pytest.mark.parametrize(_PARAMS, _CASES, ids=_IDS)
def test_correct_out_dtype_is_written(
    name: str,
    make: Callable[[], Any],
    invoke: _Invoke,
    dt: Any,
    max_out_attr: str,
    floor: int,
) -> None:
    """The counterweight to the reject test: a correct buffer IS filled.

    Without this, an implementation that rejected every ``out=`` buffer — or
    quietly ignored it and returned a fresh array — would pass the reject test
    while breaking the contract just as badly.
    """
    obj = make()
    n = _out_len(obj, max_out_attr, floor)
    sentinel = np.full(n, 7, dtype=dt)
    invoke(obj, sentinel)
    assert not np.all(sentinel == 7), f"{name} did not write the caller's out="
