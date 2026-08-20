"""Binding-level tests for ``doppler.viterbi.Viterbi``.

The ALGORITHM is pinned in C — ``native/tests/test_viterbi_core.c`` sweeps k
from 3 to 9 and n from 1 to 3, measures the code's published free distance and
holds the node-synchronization metric to the channel's symbol error rate.
Repeating any of that here would only re-measure the same kernel through a
narrower door.  What only Python can see is the DOOR: what the constructor
accepts and refuses, what dtype and length come back, and that ``reset`` and
the context manager reach the C functions they claim to.

The one numeric claim below needs no encoder and no golden vector.  The
identity code — ``k = 2``, ``n = 1``, ``poly = [0b10]`` — taps only the newest
input bit, so its encoder is the identity and a maximum-likelihood decode of
it is exactly a hard slicer.  ``decode`` must therefore return ``llr < 0``
element for element, which is a statement about the LLR sign convention alone.
That makes it checkable against arithmetic rather than against a fixture this
file built, and it is the same device §5b uses on the C side.

Serialization is not tested here: ``Viterbi`` is a row in the shared matrix at
``src/doppler/tests/test_state_serialization.py``, which asserts bit-exact
mid-stream resume and the envelope rejects for every serializable type at once.
"""

import numpy as np
import pytest

from doppler.coding import Viterbi

# CCSDS 131.0-B-3 section 3's inner code: G1 = 171, G2 = 133 octal.
CCSDS_POLY = np.array([0o171, 0o133], dtype=np.uint32)

# The identity code: one output tapping only the newest input bit.
IDENT_POLY = np.array([0b10], dtype=np.uint32)


def test_ccsds_code_is_constructible():
    """The configuration this component exists to serve, by keyword and by
    position, with the defaults doing what the manifest says."""
    v = Viterbi(CCSDS_POLY, k=7, invert=0, depth=35)
    assert v is not None
    # k and depth default to the CCSDS values, so the short form is the same
    # decoder — asserted rather than assumed, since a default that drifted
    # would silently give a caller a different code.
    assert Viterbi(CCSDS_POLY).state_bytes() == v.state_bytes()


def test_a_plain_list_is_accepted():
    """The polynomials are octal literals a caller types, not an array they
    already hold, so the ergonomic form must work."""
    v = Viterbi([0o171, 0o133], k=7, depth=35)
    assert v.state_bytes() == Viterbi(CCSDS_POLY, k=7, depth=35).state_bytes()


@pytest.mark.parametrize(
    ("poly", "k", "depth", "why"),
    [
        ([], 7, 35, "no polynomials is not a code"),
        ([0], 7, 35, "a zero polynomial is an output carrying nothing"),
        ([0o400], 7, 35, "a polynomial wider than the register"),
        ([0o171, 0o133], 1, 35, "k below the smallest register"),
        ([0o171, 0o133], 99, 35, "k past the largest supported"),
        ([0o171, 0o133], 7, 0, "depth 0 is not a decoder"),
    ],
)
def test_an_unusable_code_raises_value_error(poly, k, depth, why):
    """A refused code is a bad ARGUMENT, not an allocation failure.  jm's
    default for a NULL ``create`` is ``MemoryError``, which sends the caller
    looking at memory pressure when the real problem is the number they typed;
    ``create_error`` in the manifest is what makes this a ``ValueError``."""
    with pytest.raises(ValueError, match="not a usable code"):
        Viterbi(np.array(poly, dtype=np.uint32), k=k, depth=depth)


def test_identity_code_decode_is_the_hard_slice():
    """Positive LLR means symbol 0 — checked against arithmetic, not a
    fixture.  Magnitudes vary so this is about the SIGN and not about a
    decoder that happens to threshold at some level."""
    depth = 8
    rng = np.random.default_rng(20260820)
    llr = ((rng.random(500) - 0.5) * (0.2 + 4.0 * rng.random(500))).astype(
        np.float32
    )
    llr[llr == 0.0] = 0.25

    v = Viterbi(IDENT_POLY, k=2, depth=depth)
    bits = np.asarray(v.decode(llr))

    # A decision needs depth-1 branches behind it, so that many are still owed.
    assert len(bits) == len(llr) - (depth - 1)
    assert bits.dtype == np.uint8
    expected = (llr[: len(bits)] < 0.0).astype(np.uint8)
    np.testing.assert_array_equal(bits, expected)
    # ...and both symbols occurred, or a decoder stuck at 0 would pass a
    # stream that happened to be all zeros.
    assert set(np.unique(bits)) == {0, 1}


def test_decode_is_continuous_across_calls():
    """The survivor ring carries between calls, so one block split in two must
    give the same bits as one call — the property `steps`-style streaming is
    for, and the one a binding that rebuilt state per call would fail."""
    rng = np.random.default_rng(7)
    llr = rng.standard_normal(1024).astype(np.float32)

    whole = np.asarray(Viterbi(CCSDS_POLY).decode(llr))
    v = Viterbi(CCSDS_POLY)
    split = np.concatenate(
        [np.asarray(v.decode(llr[:400])), np.asarray(v.decode(llr[400:]))]
    )
    np.testing.assert_array_equal(whole, split)


def test_reset_returns_the_decoder_to_its_start():
    """`reset` must clear the path metrics and the ring, not merely rewind a
    counter: decoding the same block after a reset must reproduce it."""
    rng = np.random.default_rng(11)
    llr = rng.standard_normal(600).astype(np.float32)

    v = Viterbi(CCSDS_POLY)
    first = np.asarray(v.decode(llr))
    v.decode(rng.standard_normal(600).astype(np.float32))  # dirty the state
    v.reset()
    np.testing.assert_array_equal(first, np.asarray(v.decode(llr)))


def test_context_manager_and_destroy():
    """Both release paths reach `viterbi_destroy`; a second `destroy` is a
    no-op rather than a double free."""
    with Viterbi(CCSDS_POLY) as v:
        assert len(np.asarray(v.decode(np.ones(256, dtype=np.float32)))) > 0

    v = Viterbi(CCSDS_POLY)
    v.destroy()
    v.destroy()
