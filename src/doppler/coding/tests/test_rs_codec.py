"""ReedSolomon — the binding, and the code the caller actually named.

`native/tests/test_rs_codec_core.c` holds the object: that the five arguments
reach the arithmetic, that a systematic codeword is placed correctly, that
every length is refused rather than read past, and that the `*_max_out`
bounds equal what the methods write. `native/tests/test_rs_core.c` holds the
CODE — the field, the distance, the decoder's radius. Neither is repeated
here.

What only this file can see:

* the BINDING — that `decode` corrects the caller's own array rather than a
  copy, that a refused code raises rather than returning something unusable,
  and that the dtype/contiguity guards are real;
* the code against an **independent** oracle: numpy's own GF(2) polynomial
  arithmetic for the generator, computed here rather than read from the
  kernel;
* and the reason this exists at all — that a Python caller can now run a
  Reed-Solomon code that is not CCSDS's.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.coding import ReedSolomon

# CCSDS 131.0-B 4.3: J = 8, F(x) = x^8+x^7+x^2+x+1, roots a^(11j),
# j = 128-E .. 127+E. Not the textbook code, and deliberately so.
CCSDS = {
    "nroots": 32,
    "field_poly": 0x87,
    "first_root": 112,
    "root_stride": 11,
}


def _gf_mul(a: int, b: int, poly: int, bits: int) -> int:
    """Carry-less multiply modulo the field polynomial, the slow way.

    Written out here rather than reached for from the kernel: an oracle that
    shares its arithmetic with its subject proves only that the subject is
    self-consistent, which every wrong Reed-Solomon also is.
    """
    r = 0
    for _ in range(bits):
        if b & 1:
            r ^= a
        b >>= 1
        a <<= 1
        if a >> bits:
            a ^= (1 << bits) | poly
    return r


def _generator(
    nroots: int, poly: int, first_root: int, stride: int, bits: int = 8
) -> np.ndarray:
    """`g(x) = prod (x - a^(stride*j))`, expanded from the definition."""
    n = (1 << bits) - 1
    # a^i for every i, by repeated multiplication by a = x.
    exp = [1] * (2 * n)
    for i in range(1, 2 * n):
        exp[i] = _gf_mul(exp[i - 1], 2, poly, bits)

    g = [1]
    for j in range(first_root, first_root + nroots):
        root = exp[(stride * j) % n]
        # multiply g(x) by (x - root); over GF(2) subtraction is XOR
        g = [0, *g]
        for i in range(len(g) - 1):
            g[i] ^= _gf_mul(g[i + 1], root, poly, bits)
    # `g[i]` is already the coefficient of `x**i`, which is the order
    # `rs_generator` reports and the order Annex G prints.
    return np.array(g, dtype=np.uint8)


# ── the code is the one the caller named ────────────────────────────────────


def test_the_generator_matches_one_expanded_from_the_definition():
    """The five numbers reach the algebra, against arithmetic written here.

    This is the check a caller makes after configuring a code from a
    document, so it is the one worth having at the Python face: the
    coefficients of `g(x)` are what standards publish.
    """
    rs = ReedSolomon(nroots=32)
    g = np.empty(rs.nroots + 1, np.uint8)
    assert rs.generator(g) == rs.nroots + 1
    assert np.array_equal(g, _generator(32, 0x1D, 1, 1))


def test_the_ccsds_root_set_gives_a_different_generator():
    """CCSDS is not the textbook code, and the polynomial says so.

    `a^11` is itself primitive, so this is a legitimate choice — and a code
    built with consecutive powers of `a` instead is a perfectly good
    RS(255,223) that no CCSDS receiver can decode.
    """
    ccsds = ReedSolomon(**CCSDS)
    g = np.empty(ccsds.nroots + 1, np.uint8)
    assert ccsds.generator(g) == 33
    assert np.array_equal(g, _generator(32, 0x87, 112, 11))

    textbook = np.empty(33, np.uint8)
    ReedSolomon(nroots=32).generator(textbook)
    assert not np.array_equal(g, textbook)


def test_a_short_generator_buffer_writes_nothing():
    """A truncated `g(x)` compares unequal to a published one for a reason
    that looks like a wrong code, so it is refused rather than truncated."""
    rs = ReedSolomon(nroots=32)
    short = np.full(rs.nroots, 0xEE, np.uint8)
    assert rs.generator(short) == 0
    assert (short == 0xEE).all()


def test_a_code_that_is_not_ccsds_now_runs_from_python():
    """The point of the slice (doppler#900 2c).

    Before this the only Reed-Solomon reachable from Python was CCSDS's,
    fixed, and only inside a `FrameDesc` stage that carries an interleaving
    depth rather than a code. RS(31,25) over GF(32) is none of those things:
    a different field, a different width, a different strength, encoded and
    decoded end to end.
    """
    rs = ReedSolomon(nroots=6, symbol_bits=5, field_poly=0b00101)
    assert (rs.n, rs.k, rs.e) == (31, 25, 3)

    info = np.arange(rs.k, dtype=np.uint8)
    word = rs.encode(info)
    assert rs.codeword_ok(word) == 1

    word[2] ^= 0x11  # symbols are 5 bits wide here
    word[19] ^= 0x07
    assert rs.decode(word) == 2
    assert np.array_equal(word[: rs.k], info)

    # ...and it is not CCSDS's code wearing a different name.
    assert rs.n != ReedSolomon(**CCSDS).n


@pytest.mark.parametrize(
    "kw,shape",
    [
        ({"nroots": 32}, (255, 223, 16)),
        ({"nroots": 16}, (255, 239, 8)),
        (dict(**CCSDS), (255, 223, 16)),
        ({"nroots": 4, "symbol_bits": 4, "field_poly": 0b0011}, (15, 11, 2)),
        ({"nroots": 6, "symbol_bits": 5, "field_poly": 0b00101}, (31, 25, 3)),
    ],
)
def test_the_derived_sizes_follow_the_field_and_the_parity(kw, shape):
    rs = ReedSolomon(**kw)
    assert (rs.n, rs.k, rs.e) == shape
    assert rs.n == 2**rs.symbol_bits - 1
    assert rs.k == rs.n - rs.nroots


# ── the refusals a round trip cannot make ───────────────────────────────────


@pytest.mark.parametrize(
    "kw,why",
    [
        (
            {"nroots": 32, "field_poly": 0x1C},
            "a non-primitive field polynomial",
        ),
        (
            {"nroots": 32, "root_stride": 5},
            "gcd(5, 255) = 5, roots not distinct",
        ),
        ({"nroots": 31}, "odd nroots is not 2E"),
        (
            {"nroots": 2, "symbol_bits": 1},
            "J below 2 is not a field to code over",
        ),
        (
            {"nroots": 32, "symbol_bits": 4, "field_poly": 0b0011},
            "no room for data",
        ),
    ],
)
def test_an_unusable_code_raises_at_construction(kw, why):
    """Both self-consistent failures are caught HERE or nowhere.

    A non-primitive polynomial and a root stride sharing a factor with `n`
    each produce arithmetic that encodes and decodes against itself
    perfectly. The constructor is the only thing that can object, which is
    why it raises ValueError naming the cause rather than MemoryError.
    """
    with pytest.raises(ValueError, match="not a usable code"):
        ReedSolomon(**kw)


def test_nroots_is_required_rather_than_defaulted():
    """There is no conventional parity count, so there is no default.

    A default would let a caller who never chose a code strength believe
    they had.
    """
    with pytest.raises(TypeError):
        ReedSolomon()


# ── the binding ─────────────────────────────────────────────────────────────


def test_encode_returns_a_whole_systematic_codeword():
    rs = ReedSolomon(nroots=32)
    info = np.arange(rs.k, dtype=np.uint8)
    word = rs.encode(info)

    assert word.dtype == np.uint8 and word.size == rs.n
    assert np.array_equal(word[: rs.k], info), "systematic"
    assert rs.codeword_ok(word) == 1
    assert not rs.syndromes(word).any()


def test_a_wrong_length_message_is_refused_rather_than_padded():
    """An empty answer, not a short codeword — the same refusal C makes."""
    rs = ReedSolomon(nroots=32)
    assert rs.encode(np.zeros(rs.k - 1, np.uint8)).size == 0
    assert rs.encode(np.zeros(rs.k + 1, np.uint8)).size == 0


def test_decode_corrects_the_callers_own_array():
    """In place, and the array the caller holds is the one that changes.

    A decode that worked on a copy would return the right count and leave
    the caller's data wrong — which is why the binding demands a writable,
    C-contiguous uint8 array instead of accepting anything convertible.
    """
    rs = ReedSolomon(nroots=32)
    info = np.arange(rs.k, dtype=np.uint8)
    word = rs.encode(info)

    word[3] ^= 0xFF
    word[40] ^= 0x01
    assert not rs.codeword_ok(word)

    assert rs.decode(word) == 2
    assert np.array_equal(word[: rs.k], info), "the caller's buffer is fixed"
    assert rs.codeword_ok(word) == 1


def test_decode_at_the_radius_and_one_past_it():
    """E errors are corrected; E+1 is not a promise the code makes.

    A refusal is not the claim "more than E errors" — beyond E a
    bounded-distance decoder can land in another codeword's sphere and
    miscorrect. So what is asserted past the radius is that the answer is
    not silently trusted, not that it is a refusal.
    """
    rs = ReedSolomon(nroots=32)
    clean = rs.encode(np.arange(rs.k, dtype=np.uint8))

    at = clean.copy()
    at[np.arange(rs.e) * 7] ^= 0xA5
    assert rs.decode(at) == rs.e
    assert np.array_equal(at, clean)

    over = clean.copy()
    over[np.arange(rs.e + 1) * 7] ^= 0xA5
    r = rs.decode(over)
    assert r < 0 or not np.array_equal(over, clean), (
        "past the radius the decoder may refuse or miscorrect, but it must "
        "not return the original codeword and call it corrected"
    )


def test_the_two_negative_codes_are_different_facts():
    """-1 is the channel's answer; -2 is the caller's mistake."""
    rs = ReedSolomon(nroots=32)
    assert rs.decode(np.zeros(rs.n - 1, np.uint8)) == -2
    assert rs.decode(np.zeros(10, np.uint8)) == -2

    rng = np.random.default_rng(0)
    noise = rng.integers(0, 256, rs.n).astype(np.uint8)
    assert rs.decode(noise) == -1 or rs.codeword_ok(noise)


def test_decode_refuses_a_read_only_array():
    """It corrects in place, so it may not accept something it cannot write.

    Silently working on a copy is the failure this guard exists to prevent.
    """
    rs = ReedSolomon(nroots=32)
    word = rs.encode(np.zeros(rs.k, np.uint8))
    word.flags.writeable = False
    with pytest.raises(TypeError):
        rs.decode(word)


def test_a_word_of_the_wrong_length_is_not_a_codeword():
    """All-zero IS a codeword of every linear code — at the right length."""
    rs = ReedSolomon(nroots=32)
    assert rs.codeword_ok(np.zeros(rs.n, np.uint8)) == 1
    assert rs.codeword_ok(np.zeros(rs.n - 1, np.uint8)) == 0
    assert rs.syndromes(np.zeros(rs.n - 1, np.uint8)).size == 0


# ── the round trip, over a channel ──────────────────────────────────────────


@pytest.mark.parametrize(
    "kw",
    [
        {"nroots": 32},
        dict(**CCSDS),
        {"nroots": 8, "symbol_bits": 5, "field_poly": 0b00101},
    ],
)
def test_every_error_pattern_within_the_radius_is_repaired(kw):
    """Random positions and random magnitudes, not a fixed pattern.

    A decoder can be wrong in a way a single hand-picked error pattern
    misses — a Forney offset that only bites at certain positions is the
    classic one, and it is exactly what `first_root != 1` introduces.
    """
    rs = ReedSolomon(**kw)
    rng = np.random.default_rng(4)
    hi = 1 << rs.symbol_bits

    for _ in range(25):
        info = rng.integers(0, hi, rs.k).astype(np.uint8)
        clean = rs.encode(info)
        word = clean.copy()

        pos = rng.choice(rs.n, size=rs.e, replace=False)
        word[pos] ^= rng.integers(1, hi, rs.e).astype(np.uint8)
        nerr = int((word != clean).sum())

        assert rs.decode(word) == nerr
        assert np.array_equal(word, clean)
