"""Uniform state-serialization contract across every serializable type.

doppler's C state ABI is uniform — ``state_bytes`` / ``get_state`` /
``set_state`` over the ``dp_state.h`` envelope — so its *Python* face should be
tested uniformly too, rather than re-deriving the same invariants in each
module's test file.  This module is the Python sibling of the C
``DP_STATE_ROUNDTRIP_TEST`` macro: one parametrized matrix that asserts, for
every type carrying the triplet:

1. **Elastic resume** — serialize mid-stream, restore into a fresh instance
   built identically, and the continuation matches an uninterrupted run
   bit-for-bit (the whole point of the interface).
2. **Self-describing rejects** — the standard envelope makes a wrong-size or
   magic-clobbered blob raise ``ValueError`` and a non-``bytes`` argument raise
   ``TypeError``, never a silent reinterpretation.

The frame-based ``Acquisition`` has a bespoke streaming shape that doesn't fit
the block-``execute`` harness; its round-trip lives in
``dsss/tests/test_acq.py`` and is intentionally not duplicated here.

Examples
--------
>>> import numpy as np
>>> from doppler.resample import RateConverter
>>> a = RateConverter(0.5)
>>> _ = a.execute(np.ones(512, dtype=np.complex64))
>>> b = RateConverter(0.5)
>>> b.set_state(a.get_state())  # resume from a's exact state
"""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

import numpy as np
import pytest
from numpy.typing import NDArray

from doppler.accumulator import AccCf64, AccF32, AccTrace
from doppler.agc import AGC
from doppler.arith import AccQ8, AccQ15
from doppler.cvt import ADC, F32ToI16, F32ToI16U32, F32ToI16U64, F32ToUQ15
from doppler.ddc import DDC, Ddcr
from doppler.delay import DelayCf64
from doppler.dsss import Despreader
from doppler.filter import FIR, HBDecimQ15
from doppler.resample import (
    CIC,
    Farrow,
    HalfbandDecimator,
    RateConverter,
    Resampler,
)
from doppler.source import AWGN, LO, NCO
from doppler.spectral import PSD, Corr, Corr2D
from doppler.track import CarrierMpsk, CarrierNda, Costas, LoopFilter
from doppler.wfm import PN

# A short real-tapped, symmetric FIR — enough delay-line state to matter.
_FIR_TAPS = (np.array([0.1, -0.2, 0.3, 0.6, 0.3, -0.2, 0.1]) + 0j).astype(
    np.complex64
)
# 4-tap halfband FIR branch (real float32) for HalfbandDecimator.
_HB_TAPS = np.array([-0.21, 0.64, 0.64, -0.21], dtype=np.float32)
# A 16-sample reference frame for the correlators / PSD (fixed frame length).
_REF16 = (np.arange(16) + 0.5j).astype(np.complex64)
# A 31-chip 0/1 spreading code for the despreader.
_CODE31 = (np.arange(31, dtype=np.uint8) & 1).astype(np.uint8)

# name -> (make, feed): `make()` builds a fresh instance; `feed(obj, seg)` runs
# one block and returns its output as an owned array (copy — some executes
# return a view into an internal buffer).  A generator (LO) ignores the segment
# values and emits len(seg) samples, so the same split logic drives every type.
_Feed = Callable[[Any, NDArray[np.complex64]], NDArray[np.complex64]]


def _acc_feed(conv: Callable[[NDArray[np.complex64]], NDArray[Any]]) -> _Feed:
    """Feed for a running accumulator: its resumable output is the total, not a
    per-sample stream, so step the block in and return the post-block
    accumulator — the matrix's continuation compare then asserts the restored
    sum matches an uninterrupted one. ``conv`` adapts the complex64 test stream
    to the accumulator's input dtype."""

    def feed(o: Any, seg: NDArray[np.complex64]) -> NDArray[np.complex64]:
        o.steps(conv(seg))
        return np.array([o.get()])

    return feed


def _despreader_feed(o: Any, seg: NDArray[np.complex64]) -> NDArray[Any]:
    """Despread a block; return the serialized state (no output stream)."""
    o.steps(seg)
    return np.frombuffer(o.get_state(), dtype=np.uint8)


def _frame_feed(method: str, n: int) -> _Feed:
    """Feed for fixed-frame accumulators (correlators / PSD): fold each
    n-sample frame through ``method`` and return the post-block state blob. The
    running accumulator is the resume observable; plans/ref are config."""

    def feed(o: Any, seg: NDArray[np.complex64]) -> NDArray[Any]:
        x = seg.astype(np.complex64)
        for i in range(0, (len(x) // n) * n, n):
            getattr(o, method)(x[i : i + n])
        return np.frombuffer(o.get_state(), dtype=np.uint8)

    return feed


def _f32_to_int_feed(o: Any, seg: NDArray[np.complex64]) -> NDArray[Any]:
    """Feed for a float→int quantizer/ADC: convert the test stream to real
    float32 and return the quantized block. ADC (dithering on) resumes its
    PRNG; the converters resume their sticky clip flag."""
    return np.array(o.steps(seg.real.astype(np.float32)))


def _delay_feed(o: Any, seg: NDArray[np.complex64]) -> NDArray[Any]:
    """Push the block through the delay line; return the final window. The ring
    + head carry across calls, so a mid-stream split must resume."""
    w: Any = None
    for v in seg:
        w = o.push_ptr(complex(v))
    return np.array(w)


def _acctrace_feed(o: Any, seg: NDArray[np.complex64]) -> NDArray[Any]:
    """Fold fixed-length frames; return the serialized state itself. AccTrace
    exposes no trace getter, so the post-block state blob *is* the observable —
    a strict whole-state resume check that works for any serializable type."""
    r = seg.real.astype(np.float32)
    n = 8  # must match the AccTrace(n=...) below
    for i in range(0, (len(r) // n) * n, n):
        o.accumulate(r[i : i + n])
    return np.frombuffer(o.get_state(), dtype=np.uint8)


CASES: dict[str, tuple[Callable[[], Any], _Feed]] = {
    "LO": (lambda: LO(0.05), lambda o, seg: np.array(o.steps(len(seg)))),
    "CIC": (lambda: CIC(4), lambda o, seg: np.array(o.decimate(seg))),
    "FIR": (lambda: FIR(_FIR_TAPS), lambda o, seg: np.array(o.execute(seg))),
    "DDC": (lambda: DDC(-0.1, 0.25), lambda o, seg: np.array(o.execute(seg))),
    # Ddcr (handle module, gh-403): real input + caller-owned output buffer.
    "Ddcr": (
        lambda: Ddcr(0.1, 0.2),
        lambda o, seg: np.array(
            o.execute(
                seg.real.astype(np.float32), np.zeros(len(seg), np.complex64)
            )
        ),
    ),
    "RateConverter": (
        lambda: RateConverter(0.5),
        lambda o, seg: np.array(o.execute(seg)),
    ),
    "Resampler": (
        lambda: Resampler(0.5),
        lambda o, seg: np.array(o.execute(seg)),
    ),
    "HalfbandDecimator": (
        lambda: HalfbandDecimator(_HB_TAPS),
        lambda o, seg: np.array(o.execute(seg)),
    ),
    # Farrow fractional-delay interpolator — the 4-tap delay line carries
    # across delay() calls, so a mid-stream split resumes.
    "Farrow": (
        lambda: Farrow("cubic"),
        lambda o, seg: np.array(o.delay(seg, 0.3)),
    ),
    # AGC — gain integrator + detector EMA + ramp memory carry across steps().
    "AGC": (
        lambda: AGC(0.0, 0.0025, 0.05),
        lambda o, seg: np.array(o.steps(seg)),
    ),
    # Running accumulators — resumable state is the total; feed returns it.
    "AccCf64": (
        lambda: AccCf64(0.0 + 0.0j),
        _acc_feed(lambda s: s.astype(np.complex128)),
    ),
    "AccF32": (
        lambda: AccF32(0.0),
        _acc_feed(lambda s: s.real.astype(np.float32)),
    ),
    "AccQ15": (
        lambda: AccQ15(0),
        _acc_feed(
            lambda s: np.clip(s.real * 1000, -32767, 32767).astype(np.int16)
        ),
    ),
    "AccQ8": (
        lambda: AccQ8(0),
        _acc_feed(lambda s: np.clip(s.real * 20, -127, 127).astype(np.int8)),
    ),
    # Float→int quantizers — sticky clip flag (and ADC's dither PRNG) resume.
    "F32ToI16": (lambda: F32ToI16(32768.0), _f32_to_int_feed),
    "F32ToI16U32": (lambda: F32ToI16U32(32768.0), _f32_to_int_feed),
    "F32ToI16U64": (lambda: F32ToI16U64(32768.0), _f32_to_int_feed),
    "F32ToUQ15": (lambda: F32ToUQ15(32768.0), _f32_to_int_feed),
    "ADC": (lambda: ADC(8, 0.0, 1), _f32_to_int_feed),
    # Field-wise objects with owned buffers — packed/unpacked element-wise.
    "DelayCf64": (lambda: DelayCf64(4), _delay_feed),
    "AccTrace": (
        lambda: AccTrace(n=8, mode="mean", alpha=0.1),
        _acctrace_feed,
    ),
    "HBDecimQ15": (
        lambda: HBDecimQ15(_HB_TAPS),
        lambda o, seg: np.array(
            o.execute(np.clip(seg.real * 1000, -32767, 32767).astype(np.int16))
        ),
    ),
    # Generators ignore the segment values, emitting len(seg) samples.
    "NCO": (
        lambda: NCO(0.01, 0),
        lambda o, seg: np.array(o.steps_u32(len(seg))),
    ),
    "AWGN": (
        lambda: AWGN(7, 1.0),
        lambda o, seg: np.array(o.generate(len(seg))),
    ),
    "PN": (
        lambda: PN(96, 1, 7),
        lambda o, seg: np.array(o.generate(len(seg))),
    ),
    # Tracking loops — carrier loops take complex baseband; LoopFilter takes a
    # real error stream (feed the segment's real part).
    "Costas": (
        lambda: Costas(0.01, 0.707, 0.0, 4, 0.0),
        lambda o, seg: np.array(o.steps(seg)),
    ),
    "CarrierMpsk": (
        lambda: CarrierMpsk(0.01, 0.707, 0.0, 4, 0.0, 4),
        lambda o, seg: np.array(o.steps(seg)),
    ),
    "CarrierNda": (
        lambda: CarrierNda(0.01, 0.707, 0.0, 4, 2, 4),
        lambda o, seg: np.array(o.steps(seg)),
    ),
    "LoopFilter": (
        lambda: LoopFilter(0.01, 0.707, 1.0),
        lambda o, seg: np.array(o.steps(seg.real.astype(np.float64))),
    ),
    # Detectors / correlators — running accumulator (or whole-struct), with the
    # opaque FFT plans + reference rebuilt by create().
    "Corr": (lambda: Corr(ref=_REF16, dwell=5), _frame_feed("execute", 16)),
    "Corr2D": (
        lambda: Corr2D(ref=_REF16.reshape(4, 4), dwell=5),
        _frame_feed("execute", 16),
    ),
    "PSD": (
        lambda: PSD(n=16, fs=1.0e6, window="hann", mode="mean", alpha=0.1),
        _frame_feed("accumulate", 16),
    ),
    "Despreader": (
        lambda: Despreader(code=_CODE31, sps=4),
        _despreader_feed,
    ),
}


def _stream(n: int, seed: int) -> NDArray[np.complex64]:
    rng = np.random.default_rng(seed)
    return (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
        np.complex64
    )


@pytest.mark.parametrize("name", list(CASES))
def test_state_resume_is_bit_exact(name: str) -> None:
    make, feed = CASES[name]
    total, cut = 2048, 777  # odd cut → mid-state split
    x = _stream(total, seed=0)

    ref = make()
    feed(ref, x[:cut])
    tail = feed(ref, x[cut:])  # uninterrupted continuation

    a = make()
    feed(a, x[:cut])
    blob = a.get_state()
    assert isinstance(blob, bytes)
    assert len(blob) == a.state_bytes()

    b = make()  # a fresh, identically-built instance
    b.set_state(blob)
    assert np.array_equal(feed(b, x[cut:]), tail)


@pytest.mark.parametrize("name", list(CASES))
def test_state_blob_is_self_validating(name: str) -> None:
    make, feed = CASES[name]
    obj = make()
    feed(obj, _stream(256, seed=1))  # warm up some state
    blob = obj.get_state()

    with pytest.raises(ValueError):  # too short
        obj.set_state(blob[:-1])
    with pytest.raises(ValueError):  # too long
        obj.set_state(blob + b"\x00")
    with pytest.raises(ValueError):  # clobbered envelope magic
        obj.set_state(bytes([blob[0] ^ 0xFF]) + blob[1:])
    with pytest.raises(TypeError):  # not bytes
        obj.set_state(42)
