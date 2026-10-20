"""A kernel that keeps a telemetry context keeps its owner alive (jm#2327).

``set_telemetry`` hands the C kernel a ``dp_tlm_t *`` that the kernel stores
and writes through on every later push. A capsule param declared
``retain = true`` makes the binding hold a strong reference to the object the
caller passed, so ``del tlm`` after the attach cannot free the pointer the
kernel still holds. Without the key the binding only borrows for the call,
and the next push writes into freed memory.

The test reads the reference count of the ``Telemetry`` directly, against
the value it had before the attach, rather than reading a freed object back.
An attach must add exactly one reference (the retained slot), feeding the
kernel must still record into it, and deleting the binding must return the
count to its baseline (the slot is dropped in dealloc). A binding that does
not retain leaves the count unchanged on attach, so the check fails cleanly.

The faces come from :data:`doppler.tests.test_tlm_prefix_refusal.FACES`,
which discovers every public class with a ``set_telemetry``, so a new
instrumented object is covered here the day it is added to that table.
"""

from __future__ import annotations

import gc
import sys

import pytest

from doppler.telemetry import Telemetry
from doppler.tests.test_tlm_prefix_refusal import FACES, _feed


@pytest.mark.parametrize("form", ["object", "capsule"])
@pytest.mark.parametrize("face", sorted(FACES))
def test_attached_telemetry_is_retained_until_the_binding_dies(
    face: str, form: str
) -> None:
    tlm = Telemetry(1 << 12)
    base = sys.getrefcount(tlm)
    obj = FACES[face]()
    arg = tlm if form == "object" else tlm._capsule
    obj.set_telemetry(arg, "p")
    del arg
    # One reference from the retained slot; the capsule form's lent
    # capsule holds tlm as its owner, so it is the same single reference.
    assert sys.getrefcount(tlm) == base + 1, "the kernel's owner is not held"

    _feed(obj)
    assert len(tlm.read()) > 0, "the retained context recorded nothing"

    del obj
    gc.collect()
    assert sys.getrefcount(tlm) == base, "the binding leaked its reference"
