"""Binding-level tests for ``doppler.coding.ConvEncoder``.

The ALGORITHM is pinned in C: ``native/tests/conv/test_conv_core.c`` holds the
encoder to its IMPULSE RESPONSE — drive a 1 and output ``j`` must trace
``poly[j]``, which is what a generator polynomial *means* — and
``test_conv_enc_core.c`` holds the object to the kernel it calls. Repeating
either here would re-measure one kernel through a narrower door.

What only Python can see is the DOOR: what the constructor takes and refuses,
what dtype and length come back, that a plain list of octal literals works,
and that the register carries across calls through the binding rather than
only inside C.

The one numeric claim below needs no golden vector, because the impulse
response is checkable against the polynomials themselves — the same external
truth the C test uses, which is why it is worth re-stating at this layer: it
proves the binding passes the code through unchanged.

Serialization is not tested here: ``ConvEncoder`` is a row in the shared
matrix at ``src/doppler/tests/test_state_serialization.py``.
"""

import numpy as np
import pytest

from doppler.coding import ConvEncoder, Viterbi

# CCSDS 131.0-B-3 section 3's inner code: G1 = 171, G2 = 133 octal, G2
# complemented.
CCSDS_POLY = np.array([0o171, 0o133], dtype=np.uint32)
CCSDS_INVERT = 0x2


def test_the_impulse_response_is_the_polynomial():
    """Checkable against arithmetic, not against a fixture this file built.

    Driving a 1 followed by zeros makes output `j` trace `poly[j]` bit for
    bit. A binding that reordered the polynomials, dropped one, or passed a
    truncated array would fail this and nothing else here would notice.
    """
    for poly, k in (([0o7, 0o5], 3), ([0o171, 0o133], 7), ([0o23], 5)):
        e = ConvEncoder(poly, k=k)
        impulse = np.zeros(k, dtype=np.uint8)
        impulse[0] = 1
        sym = np.asarray(e.encode(impulse)).reshape(-1, len(poly))
        for j, p in enumerate(poly):
            traced = int("".join(str(int(b)) for b in sym[:, j]), 2)
            assert traced == p, f"output {j} traced {traced:o}, want {p:o}"


def test_a_plain_list_is_accepted():
    """The polynomials are octal literals a caller types, not an array they
    already hold, so the ergonomic form must work and must agree."""
    a = ConvEncoder([0o171, 0o133], k=7, invert=CCSDS_INVERT)
    b = ConvEncoder(CCSDS_POLY, k=7, invert=CCSDS_INVERT)
    x = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)
    np.testing.assert_array_equal(
        np.asarray(a.encode(x)), np.asarray(b.encode(x))
    )


def test_invert_is_the_callers_and_changes_the_stream():
    """CCSDS complements G2; most codes complement nothing.

    An encoder that ignored the mask would round-trip perfectly against a
    decoder that also ignored it, and interoperate with nothing — which is
    why this asserts the streams DIFFER rather than that either is correct.
    """
    x = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 8, dtype=np.uint8)
    plain = np.asarray(ConvEncoder(CCSDS_POLY, k=7).encode(x))
    ccsds = np.asarray(
        ConvEncoder(CCSDS_POLY, k=7, invert=CCSDS_INVERT).encode(x)
    )
    assert not np.array_equal(plain, ccsds)
    # ...and it is exactly G2's column that moved, which the mask names.
    assert np.array_equal(plain[0::2], ccsds[0::2])
    assert np.array_equal(plain[1::2] ^ 1, ccsds[1::2])


@pytest.mark.parametrize(
    ("poly", "k", "why"),
    [
        ([], 7, "no polynomials is not a code"),
        ([0], 7, "a zero polynomial is an output carrying nothing"),
        ([0o400], 7, "a polynomial wider than the register"),
        ([0o171, 0o133], 1, "k below the smallest register"),
        ([0o171, 0o133], 99, "k past the largest supported"),
    ],
)
def test_an_unusable_code_raises_value_error(poly, k, why):
    """A refused code is a bad ARGUMENT, not an allocation failure. jm's
    default for a NULL ``create`` is ``MemoryError``, which sends the caller
    looking at memory pressure when the problem is the number they typed."""
    with pytest.raises(ValueError, match="not a usable code"):
        ConvEncoder(np.array(poly, dtype=np.uint32), k=k)


def test_output_shape_and_dtype():
    """Rate 1/n with no fill and no latency — the asymmetry with the decoder,
    which still owes bits while its traceback fills."""
    for poly in ([0o7, 0o5], [0o171, 0o165, 0o133], [0o23]):
        x = np.zeros(64, dtype=np.uint8)
        k = 5 if len(poly) == 1 else 7
        sym = np.asarray(ConvEncoder(poly, k=k).encode(x))
        assert sym.dtype == np.uint8
        assert sym.size == x.size * len(poly)


def test_the_register_carries_across_calls():
    """One block split in two must equal one call, and a reset must break it.

    The second half is what makes the first non-vacuous: an encoder that
    never carried anything would pass the equality and fail this.
    """
    rng = np.random.default_rng(7)
    x = rng.integers(0, 2, 512).astype(np.uint8)

    whole = np.asarray(ConvEncoder(CCSDS_POLY, k=7).encode(x))
    e = ConvEncoder(CCSDS_POLY, k=7)
    split = np.concatenate(
        [np.asarray(e.encode(x[:137])), np.asarray(e.encode(x[137:]))]
    )
    np.testing.assert_array_equal(whole, split)

    d = ConvEncoder(CCSDS_POLY, k=7)
    first = np.asarray(d.encode(x[:137]))
    d.reset()
    restarted = np.concatenate([first, np.asarray(d.encode(x[137:]))])
    assert not np.array_equal(whole, restarted)


def test_it_round_trips_through_the_decoder():
    """The point of doppler#900, at the layer a user meets it.

    `Viterbi` accepted any rate-1/n code and Python could produce symbols for
    exactly one of them. Both directions now take the same polynomials, so a
    caller can close the loop on a code of their own.
    """
    rng = np.random.default_rng(20260820)
    for poly, k in (([0o7, 0o5], 3), ([0o171, 0o133], 7)):
        bits = rng.integers(0, 2, 400).astype(np.uint8)
        sym = np.asarray(ConvEncoder(poly, k=k).encode(bits))
        llr = np.where(sym, -8.0, 8.0).astype(np.float32)
        out = np.asarray(Viterbi(poly, k=k, depth=5 * k + 25).decode(llr))
        assert out.size > 0
        np.testing.assert_array_equal(out, bits[: out.size])


def test_context_manager_and_destroy():
    """Both release paths reach `dp_conv_enc_destroy`; a second `destroy` is a
    no-op rather than a double free."""
    with ConvEncoder(CCSDS_POLY, k=7) as e:
        assert np.asarray(e.encode(np.zeros(8, dtype=np.uint8))).size == 16

    e = ConvEncoder(CCSDS_POLY, k=7)
    e.destroy()
    e.destroy()


# A rate-3/4 SHAPE, not any standard's pattern: period 3, keep mother indices
# 0 1 2 5 of each six.
P34 = {"punct_period": 3, "punct_mask": 0b100111}


def test_a_punctured_stream_is_the_mother_stream_with_positions_struck():
    """The expected stream is the UNPUNCTURED encoder's output with the
    listed indices kept, so the oracle is the encoder with no pattern and a
    literal list, never the pattern arithmetic under test."""
    bits = np.random.default_rng(3).integers(0, 2, 60).astype(np.uint8)
    mother = ConvEncoder(CCSDS_POLY, k=7, invert=CCSDS_INVERT).encode(bits)
    keep = np.array([0, 1, 2, 5])
    want = np.concatenate([mother[6 * i + keep] for i in range(20)])
    got = ConvEncoder(CCSDS_POLY, k=7, invert=CCSDS_INVERT, **P34).encode(bits)
    np.testing.assert_array_equal(got, want)


def test_the_pattern_phase_carries_across_calls():
    """A split that ends mid-pattern must not restart it."""
    bits = np.random.default_rng(4).integers(0, 2, 61).astype(np.uint8)
    whole = ConvEncoder(CCSDS_POLY, k=7, **P34).encode(bits)
    e = ConvEncoder(CCSDS_POLY, k=7, **P34)
    split = np.concatenate([e.encode(bits[:8]), e.encode(bits[8:])])
    np.testing.assert_array_equal(whole, split)


def test_a_punctured_stream_decodes_through_the_matching_viterbi():
    bits = np.random.default_rng(5).integers(0, 2, 200).astype(np.uint8)
    sym = ConvEncoder(CCSDS_POLY, k=7, invert=CCSDS_INVERT, **P34).encode(bits)
    llr = np.where(sym == 1, -4.0, 4.0).astype(np.float32)
    out = Viterbi(
        CCSDS_POLY, k=7, invert=CCSDS_INVERT, depth=60, **P34
    ).decode(llr)
    np.testing.assert_array_equal(out, bits[: out.size])
    assert out.size == 200 - 59


@pytest.mark.parametrize(
    "bad",
    [
        {"punct_period": 3},  # a period with no mask
        {"punct_mask": 0x7},  # a mask with no period
        {"punct_period": 3, "punct_mask": 0x33},  # an input keeps nothing
    ],
)
def test_a_pattern_that_is_not_one_is_refused(bad):
    with pytest.raises(ValueError):
        ConvEncoder(CCSDS_POLY, k=7, **bad)
    with pytest.raises(ValueError):
        Viterbi(CCSDS_POLY, k=7, **bad)


def test_a_decoder_refuses_a_length_that_ends_inside_an_input():
    """The refusal is the kernel's own, and the binding passes it through as
    it does for an unpunctured code: nothing is decoded and nothing moves, so
    the next whole block decodes exactly as if the refusal never happened."""
    bits = np.random.default_rng(6).integers(0, 2, 100).astype(np.uint8)
    sym = ConvEncoder(CCSDS_POLY, k=7, **P34).encode(bits)
    llr = np.where(sym == 1, -4.0, 4.0).astype(np.float32)

    v = Viterbi(CCSDS_POLY, k=7, depth=10, **P34)
    assert v.decode(llr[:1]).size == 0  # half of input 0
    ref = Viterbi(CCSDS_POLY, k=7, depth=10, **P34).decode(llr)
    np.testing.assert_array_equal(v.decode(llr), ref)
