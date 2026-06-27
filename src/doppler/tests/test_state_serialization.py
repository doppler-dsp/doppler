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

from doppler.ddc import DDC
from doppler.filter import FIR
from doppler.resample import CIC, RateConverter
from doppler.source import LO

# A short real-tapped, symmetric FIR — enough delay-line state to matter.
_FIR_TAPS = (np.array([0.1, -0.2, 0.3, 0.6, 0.3, -0.2, 0.1]) + 0j).astype(
    np.complex64
)

# name -> (make, feed): `make()` builds a fresh instance; `feed(obj, seg)` runs
# one block and returns its output as an owned array (copy — some executes
# return a view into an internal buffer).  A generator (LO) ignores the segment
# values and emits len(seg) samples, so the same split logic drives every type.
_Feed = Callable[[Any, NDArray[np.complex64]], NDArray[np.complex64]]
CASES: dict[str, tuple[Callable[[], Any], _Feed]] = {
    "LO": (lambda: LO(0.05), lambda o, seg: np.array(o.steps(len(seg)))),
    "CIC": (lambda: CIC(4), lambda o, seg: np.array(o.decimate(seg))),
    "FIR": (lambda: FIR(_FIR_TAPS), lambda o, seg: np.array(o.execute(seg))),
    "DDC": (lambda: DDC(-0.1, 0.25), lambda o, seg: np.array(o.execute(seg))),
    "RateConverter": (
        lambda: RateConverter(0.5),
        lambda o, seg: np.array(o.execute(seg)),
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
