"""SyncFinder — the binding, and the thing it makes possible.

`native/tests/test_syncword_core.c` holds the kernel: generality across
marker lengths, first-below-threshold, the polarity arithmetic, and the
false-alarm formula against an exhaustive count. None of that is repeated
here — two files agreeing about one kernel is not two checks.

What only this file can see:

* the BINDING — that the record arrives named, that a miss is legible, that
  a refused marker raises rather than returning something unusable;
* the closed form against a SECOND, independent oracle in a different
  language (`math.comb`), which is the check the C test cannot be, since its
  oracle and its subject were compiled together;
* and the point of the whole slice: that a Python receiver can now ACQUIRE a
  CADU rather than only check one it was handed.
"""

from __future__ import annotations

import math

import numpy as np
import pytest

from doppler.detection import SyncFinder
from doppler.wfm import FrameDesc, ccsds_asm_bits

EMPTY = np.empty(0, np.uint8)


# ── the marker reaches Python without being transcribed ─────────────────────


def test_the_asm_is_the_published_constant_msb_first():
    """0x1ACFFC1D, first transmitted bit at the top of 0x1A (figure 9-1).

    Spelled out here as an integer comparison rather than as a second
    bit-by-bit expansion, so this is a check against the STANDARD and not a
    restatement of the code under test.
    """
    b = ccsds_asm_bits()
    assert b.dtype == np.uint8
    assert b.size == 32
    assert int("".join(map(str, b.tolist())), 2) == 0x1ACFFC1D


def test_the_asm_is_a_fresh_array_each_call():
    """A caller may keep, mutate or free it without touching the next one."""
    a, b = ccsds_asm_bits(), ccsds_asm_bits()
    a[:] = 0
    assert b.any()


# ── the binding ─────────────────────────────────────────────────────────────


def test_an_empty_marker_is_refused_with_a_reason():
    """It would match at every offset with zero errors — frame sync, forever.

    A ValueError naming the cause rather than the blanket MemoryError a NULL
    from `create()` would otherwise become (jm gh-482).
    """
    with pytest.raises(ValueError, match="non-empty"):
        SyncFinder(EMPTY)


def test_the_hit_is_a_named_record():
    """offset, polarity and distance are ONE answer.

    A receiver that took the offset without the polarity would hand its frame
    decoder a complemented stream and read every bit backwards, silently.
    """
    asm = ccsds_asm_bits()
    f = SyncFinder(asm)
    hit = f.find(np.concatenate([np.zeros(96, np.uint8), asm]), max_errors=0)
    assert (hit.found, hit.offset, hit.inverted, hit.errors) == (1, 96, 0, 0)
    assert tuple(hit) == (1, 96, 0, 0)
    assert type(hit).__name__ == "SyncHit"


def test_a_miss_is_legible_rather_than_a_sentinel_offset():
    """Offset 0 is a perfectly good place for a marker to be.

    So a miss cannot be spelled as one, and `found` is what the other three
    fields mean nothing without.
    """
    f = SyncFinder(ccsds_asm_bits())
    hit = f.find(np.zeros(4096, np.uint8), max_errors=0)
    assert hit.found == 0
    assert (hit.offset, hit.inverted, hit.errors) == (0, 0, 0)


def test_nbits_reports_the_marker_it_was_built_from():
    assert SyncFinder(ccsds_asm_bits()).nbits == 32
    assert SyncFinder(np.ones(7, np.uint8)).nbits == 7


def test_the_searcher_outlives_the_array_it_was_built_from():
    """`SyncFinder(ccsds_asm_bits())` is the ordinary spelling.

    numpy frees that temporary the moment the constructor returns, so a
    searcher holding the caller's pointer would search whatever landed there
    next — and would do it without complaining.
    """
    marker = np.array([1, 0, 1, 1, 0, 1, 0, 0, 1], np.uint8)
    f = SyncFinder(marker)
    want = marker.copy()
    marker[:] = 0

    rx = np.zeros(60, np.uint8)
    rx[25:34] = want
    assert (f.find(rx, max_errors=0).found, f.find(rx, 0).offset) == (1, 25)


# ── the arithmetic, against an oracle written in a different language ───────


@pytest.mark.parametrize("n", [8, 13, 32])
@pytest.mark.parametrize("t", [0, 1, 2, 3])
def test_pfa_matches_the_closed_form_computed_independently(n, t):
    """`2 * sum_{i<=t} C(n,i) / 2^n`, from `math.comb` rather than lgamma.

    The C test checks this against an exhaustive enumeration, but its oracle
    and its subject were compiled together and share a notion of what a
    double is. This one is exact integer arithmetic in another language.
    """
    f = SyncFinder(np.ones(n, np.uint8))
    want = 2 * sum(math.comb(n, i) for i in range(t + 1)) / 2**n
    assert f.pfa(t) == pytest.approx(min(want, 1.0), rel=1e-12)


def test_pfa_saturates_once_every_window_matches_a_polarity():
    """At 2t >= n a window is within t of the marker or of its complement.

    The closed form's factor of two double-counts there, and a probability
    above 1 handed to `1 - (1-p)**W` is a domain error, not a large number.
    """
    f = SyncFinder(np.ones(8, np.uint8))
    assert f.pfa(4) == 1.0
    assert f.pfa(8) == 1.0


def test_the_threshold_falls_as_the_search_window_grows():
    """doppler#897, as an API rather than as a warning.

    A caller reading only the marker length picks 8 from "half of 32", and
    what they actually need depends on how much stream they sweep first.
    """
    f = SyncFinder(ccsds_asm_bits())
    got = [f.max_errors_for(w, pfa=1e-3) for w in (32, 96, 4096, 100_000)]
    assert got == sorted(got, reverse=True)
    assert got[0] > got[-1], "a longer window must not afford MORE tolerance"


def test_the_threshold_is_the_largest_one_that_holds():
    """Tight from both sides — either alone is met by a constant.

    Too strict and the caller loses frames the channel only grazed; one step
    looser and they get the false-frame rate they asked not to have.
    """
    f = SyncFinder(ccsds_asm_bits())
    w, target = 4096, 1e-3
    t = f.max_errors_for(w, pfa=target)
    assert 1 - (1 - f.pfa(t)) ** w <= target
    assert 1 - (1 - f.pfa(t + 1)) ** w > target


def test_an_unachievable_rate_is_reported_rather_than_rounded_to_zero():
    """0 would read as "exact matches only", which a caller would act on."""
    f = SyncFinder(ccsds_asm_bits())
    assert f.max_errors_for(10**12, pfa=1e-9) == -1


# ── the point of the slice ──────────────────────────────────────────────────


def _cadu(depth: int = 2) -> tuple[FrameDesc, np.ndarray]:
    """A CCSDS codeblock behind its marker, described and materialised.

    No inner code: frame synchronisation happens after the Viterbi, which is
    also where `check()` begins.
    """
    K, E2, RS, RANDOMISE = 223, 32, 1, 2
    octets = np.array(
        [(i * 37 + 11) & 0xFF for i in range(K * depth)], np.uint8
    )
    d = FrameDesc(EMPTY, EMPTY, EMPTY)
    d.add_field(ccsds_asm_bits())
    d.add_field(np.unpackbits(octets).astype(np.uint8))
    d.add_field(EMPTY, derived_by=1, derived_bits=E2 * depth * 8)
    d.add_stage(RS, first_field=1, n_fields=2, depth=depth)
    d.add_stage(RANDOMISE, first_field=1, n_fields=2)
    d.build()
    return d, np.asarray(d.bits(1))


@pytest.mark.parametrize("invert", [False, True])
def test_a_python_receiver_can_acquire_a_cadu_and_then_check_it(invert):
    """Acquire, then check — the two halves that had never met in Python.

    `Frame.check()` has always been able to score a frame it was HANDED.
    Nothing could find one: `ccsds_tm_asm_find` had no binding, so a Python
    receiver holding a bit stream had no way to learn where a CADU starts,
    and the whole threshold story doppler#897 measured described a function
    it could not call.

    The inverted case is not a variation on the same test. A BPSK carrier
    recovered through a 180-degree ambiguity delivers every bit complemented,
    and the marker — which no randomiser covers — is the only thing in a CADU
    that can say so. Without that flag the frame is acquired at exactly the
    right offset and then scored as garbage.
    """
    desc, frame = _cadu()
    lead = 96
    rng = np.random.default_rng(7)
    stream = np.concatenate([rng.integers(0, 2, lead).astype(np.uint8), frame])
    if invert:
        stream = (stream ^ 1).astype(np.uint8)

    f = SyncFinder(ccsds_asm_bits())
    hit = f.find(stream, max_errors=f.max_errors_for(lead, pfa=1e-3))

    assert hit.found, "the marker is there and within tolerance"
    assert hit.offset == lead
    assert bool(hit.inverted) is invert

    rx = stream[hit.offset : hit.offset + desc.nbits]
    if hit.inverted:
        rx = (rx ^ 1).astype(np.uint8)

    # The assertion that bites is this one, not `check()` -- see
    # test_the_outer_code_cannot_see_an_inversion_but_the_marker_can.
    assert np.array_equal(rx, frame)

    r = desc.check(rx)
    assert (r.passed, r.corrected, r.symbols) == (1, 0, 0)


def test_the_outer_code_cannot_see_an_inversion_but_the_marker_can():
    """Why `inverted` is a field and not a curiosity, measured.

    A complemented CADU passes its own outer code, cleanly, with nothing
    corrected. Reed-Solomon is linear and the all-ones vector is itself a
    full-length codeword -- every symbol's evaluation at the generator's
    roots sums to zero over the whole period -- so complementing a codeword
    lands on another codeword, and the decoder has nothing to object to. The
    randomiser is an XOR and carries the complement straight through.

    So a receiver that acquired at the right offset and ignored the polarity
    would score a PASS on a frame whose every payload bit is wrong. Nothing
    downstream can catch that. The marker can, because no randomiser covers
    it: it reads the same in every frame and in exactly one polarity.

    The header has said this in prose since the component was written. This
    is the first thing that runs it.
    """
    desc, frame = _cadu()
    flipped = (frame ^ 1).astype(np.uint8)

    r = desc.check(flipped)
    assert r.passed == 1, "the outer code is blind to a global complement"
    assert (r.corrected, r.symbols) == (0, 0), "...and does not even work"

    hit = SyncFinder(ccsds_asm_bits()).find(flipped, max_errors=0)
    assert (hit.found, hit.offset, hit.inverted) == (1, 0, 1)


def test_acquisition_survives_the_errors_the_outer_code_then_repairs():
    """The threshold is not decoration: a channel touches the marker too.

    A tolerance of zero loses this frame at the sync search even though the
    Reed-Solomon behind it repairs the payload without effort — the failure
    mode the header calls "misses a frame the channel touched", and the
    reason `max_errors_for` exists rather than a note saying "pick one".

    The payload is damaged too, and separately, because the two errors are
    unrelated: the marker is NOT covered by the outer code (9.5.1), so
    surviving the sync search and being repairable are independent facts and
    a test that damaged only the marker would show neither.
    """
    desc, frame = _cadu()
    lead = 96
    rng = np.random.default_rng(11)
    stream = np.concatenate([rng.integers(0, 2, lead).astype(np.uint8), frame])
    stream[lead + 3] ^= 1  # in the marker: costs acquisition tolerance
    stream[lead + 20] ^= 1
    stream[lead + 500] ^= 1  # in the codeblock: costs the outer code a repair
    stream[lead + 900] ^= 1

    f = SyncFinder(ccsds_asm_bits())
    assert f.find(stream, max_errors=0).found == 0, (
        "an exact-match search loses a frame the outer code would have kept"
    )

    hit = f.find(stream, max_errors=f.max_errors_for(lead, pfa=1e-3))
    assert (hit.found, hit.offset, hit.errors) == (1, lead, 2)

    r = desc.check(stream[hit.offset : hit.offset + desc.nbits])
    assert r.passed == 1
    assert r.corrected > 0, "the outer code did real work, and says so"
    assert r.symbols >= 2
