"""An array argument refuses a ``str`` rather than reading it as numbers.

Objects take bits; module helpers make them (``field_bits(text)``,
``cvt.hex_to_bin``, ``cvt.bytes_to_bin``). A ``str`` handed to an array
parameter used to reach a bare ``PyArray_FROM_OTF``, which PARSES it:
``Frame(sync="0101")`` became the number 101 and failed later, if at all,
with a reason about something else. just-makeit#1700 gave every generated
array argument one converter, ``jm_array_arg``, that refuses a ``str`` by
parameter name -- but a sacred ``_ext_<obj>.c`` fragment is never re-rendered,
so it only reaches a fragment that is regenerated (doppler#1654).

This matrix is the gate, in the shape of ``test_out_param_dtype.py``: one
entry per array parameter, each asserting both halves, so it cannot pass by
refusing everything:

1. **A ``str`` raises ``TypeError`` naming the parameter**, never a number,
   and **names ``field_bits()``**, the door text takes (doppler#1708). Each
   parameter declares just-makeit#1756's ``str_hint`` in
   ``objects/frame.toml``, which jm appends to the refusal.
2. **The bits themselves are taken**: the same call with a ``uint8`` array
   (and with ``bytes``, which the stub promises) does not raise.

The hint is the sentence a composer source's bit field refuses text with
(``dp_wfm_source_bits_refuse_text``). A TOML string cannot point at a C
string, so the manifest holds a copy. The test does not restate that
sentence: it reads it from the composer's own refusal at runtime and
requires each binding's message to end with it, so the copies cannot drift
apart.

Examples
--------
>>> from doppler.wfm import Frame
>>> Frame(sync="0101")  # doctest: +NORMALIZE_WHITESPACE
Traceback (most recent call last):
    ...
TypeError: sync must be an array of numbers, not str: a bit field takes
bits (a uint8 array); build them from text with field_bits()
"""

from __future__ import annotations

import re
from typing import TYPE_CHECKING, Any

import numpy as np
import pytest

from doppler.wfm import Frame, FrameDesc

if TYPE_CHECKING:
    from collections.abc import Callable

SYNC = np.array([1, 1, 1, 0, 0, 1, 0], np.uint8)
PAYLOAD = np.array([0, 1, 1, 0, 1, 0, 0, 1], np.uint8)


def _built(cls: type) -> Any:
    """A materialised frame: `Frame` builds on construction, `FrameDesc`
    (the deferred flavour) on `build()`."""
    f = cls(np.zeros(0, np.uint8), SYNC, PAYLOAD, crc="crc16")
    if cls is FrameDesc:
        f.build()
    return f


def _rx(cls: type) -> np.ndarray:
    return np.asarray(_built(cls).bits(), np.uint8)


#: (label, the parameter's name, a call that passes `v` to it)
CASES: list[tuple[str, str, Callable[[Any], Any]]] = []
# add_field extends a DESCRIPTION; a Frame is built on construction.
CASES.append(
    ("FrameDesc.add_field", "bits", lambda v: FrameDesc().add_field("f", v))
)
for _cls in (Frame, FrameDesc):
    _n = _cls.__name__
    CASES += [
        (f"{_n}(preamble=)", "preamble", lambda v, c=_cls: c(preamble=v)),
        (f"{_n}(sync=)", "sync", lambda v, c=_cls: c(sync=v)),
        (f"{_n}(payload=)", "payload", lambda v, c=_cls: c(payload=v)),
        (
            f"{_n}.crc_ok",
            "rx_bits",
            lambda v, c=_cls: _built(c).crc_ok(v),
        ),
        (
            f"{_n}.deframe",
            "rx_bits",
            lambda v, c=_cls: _built(c).deframe(v),
        ),
        (f"{_n}.check", "rx_bits", lambda v, c=_cls: _built(c).check(v)),
    ]


def _good(label: str, cls: type) -> np.ndarray:
    """Bits the call can take: a received frame for the receive methods."""
    if label.endswith(("crc_ok", "deframe", "check")):
        return _rx(cls)
    return SYNC


def _composer_reason() -> str:
    """The reason a composer source's bit field refuses text: the C
    sentence (``dp_wfm_source_bits_refuse_text``) each ``str_hint`` copies.
    """
    from doppler.wfm import Synth

    with pytest.raises(ValueError) as exc:
        Synth(type="dsss", data_code="0101")
    return str(exc.value)


@pytest.mark.parametrize(
    ("label", "param", "call"), CASES, ids=[c[0] for c in CASES]
)
def test_a_str_is_refused_by_name(label, param, call):
    with pytest.raises(TypeError, match=rf"^{param} must be an array"):
        call("0101")


@pytest.mark.parametrize(
    ("label", "param", "call"), CASES, ids=[c[0] for c in CASES]
)
def test_the_refusal_names_field_bits(label, param, call):
    """doppler#1708: the refusal says where text goes, in the composer's
    words, so a str meets one reason whichever object it reaches."""
    reason = _composer_reason()
    assert "field_bits()" in reason
    with pytest.raises(TypeError) as exc:
        call("0101")
    assert str(exc.value) == (
        f"{param} must be an array of numbers, not str: {reason}"
    )


@pytest.mark.parametrize(
    ("label", "param", "call"), CASES, ids=[c[0] for c in CASES]
)
def test_the_bits_are_taken(label, param, call):
    cls = FrameDesc if label.startswith("FrameDesc") else Frame
    bits = _good(label, cls)
    call(bits)
    call(bits.tobytes())  # the stub says bytes; the runtime agrees


FRAGMENTS = ("wfm_ext_frame.c", "wfm_ext_framedesc.c")


@pytest.mark.parametrize("name", FRAGMENTS)
def test_the_fragment_converts_through_jm_array_arg(name):
    """The matrix lists calls; this reads the binding, so a parameter added
    later cannot reach a bare `PyArray_FROM_OTF` unlisted (doppler#1654)."""
    from doppler.tests._repo import repo_root

    src = (repo_root(__file__) / "native/src/wfm" / name).read_text(
        encoding="utf-8"
    )
    assert "jm_array_arg" in src
    assert not re.search(
        r"=\s*\(PyArrayObject \*\)\s*PyArray_FROM_OTF", src
    ), (
        f"{name} converts an argument with a bare PyArray_FROM_OTF, which "
        "parses a str as numbers: regenerate it (delete + make jm-apply)"
    )
