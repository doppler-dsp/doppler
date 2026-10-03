"""The burst receivers refuse a ``str`` rather than reading it as numbers.

``BurstDemod`` and ``DsssBurstReceiver`` took their code arrays through a
bare ``PyArray_FROM_OTF``, which PARSES a ``str``: ``acq_code="0101"``
became the number 101 and failed later, if at all, with a reason about
something else. Their bindings were sacred (hand-owned) fragments, which are
never re-rendered, so just-makeit#1700's converter never reached them.
Regenerating them under the current template (doppler#1620 step 3) does, and
this is the gate, in the shape of ``test_array_arg_refuses_str.py``: each
bit-code parameter (the ones that declare just-makeit's ``str_hint``) gets a
``str`` and must raise a ``TypeError`` naming the parameter and
``field_bits()``; the same call with the bits taken (a ``uint8`` array, and
``bytes``) must not raise, so it cannot pass by refusing everything. The
sample input ``x`` declares no hint, so a ``str`` there converts the way numpy
does: refusing it is not part of this contract.

Examples
--------
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> from doppler.wfm import Frame
>>> f = Frame(
...     sync=np.zeros(13, np.uint8), payload=np.zeros(3, np.uint8), crc="crc16"
... )
>>> BurstDemod("0101", f)  # doctest: +NORMALIZE_WHITESPACE
Traceback (most recent call last):
    ...
TypeError: data_code must be an array of numbers, not str: a bit field
takes bits (a uint8 array); build them from text with field_bits()
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.dsss import BurstDemod, DsssBurstReceiver
from doppler.wfm import Frame

FRAME = Frame(
    sync=np.zeros(13, np.uint8), payload=np.zeros(3, np.uint8), crc="crc16"
)
CODE = (np.arange(31) & 1).astype(np.uint8)
DCODE = (np.arange(8) & 1).astype(np.uint8)
HINT = "field_bits()"


def _demod(**kw):
    return BurstDemod(kw.pop("data_code", DCODE), FRAME, **kw)


def _rx(**kw):
    return DsssBurstReceiver(
        kw.pop("acq_code", CODE),
        kw.pop("data_code", DCODE),
        FRAME,
        reps=4,
        spc=4,
        **kw,
    )


#: (label, parameter, a call passing `v` to it, whether the bits hint is
#: expected in the message). Only the bit codes: they declare a `str_hint`.
CASES = [
    ("BurstDemod(data_code=)", "data_code", lambda v: _demod(data_code=v), 1),
    (
        "BurstDemod.set_preamble(acq_code=)",
        "acq_code",
        lambda v: _demod().set_preamble(v, 5),
        1,
    ),
    ("DsssBurstReceiver(acq_code=)", "acq_code", lambda v: _rx(acq_code=v), 1),
    (
        "DsssBurstReceiver(data_code=)",
        "data_code",
        lambda v: _rx(data_code=v),
        1,
    ),
]


@pytest.mark.parametrize(
    ("label", "param", "call", "hinted"), CASES, ids=[c[0] for c in CASES]
)
def test_a_str_is_refused_by_name(label, param, call, hinted):
    with pytest.raises(TypeError, match=rf"^{param} must be an array") as exc:
        call("0101")
    assert (HINT in str(exc.value)) == bool(hinted), str(exc.value)


@pytest.mark.parametrize(
    ("label", "param", "call", "hinted"),
    [c for c in CASES if c[3]],
    ids=[c[0] for c in CASES if c[3]],
)
def test_the_bits_themselves_are_taken(label, param, call, hinted):
    call(CODE)  # a uint8 array
    call(bytes(CODE.tobytes()))  # and bytes, which the stub promises
