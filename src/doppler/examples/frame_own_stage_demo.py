#!/usr/bin/env python3
"""frame_own_stage_demo.py -- a frame that is nobody's standard, and a
transform doppler has never heard of, described and checked from Python.

Two things this exists to show, and the second is the interesting one.

**A generic frame.** A header of the caller's own bits, a payload, and a
CRC-16 over a span named rather than counted. No standard appears anywhere
in it. Every other worked frame example in this tree is a CCSDS CADU, which
teaches the standard rather than the descriptor -- the descriptor is
general, and CCSDS is one configuration of it.

**A stage kind that is YOURS.** ``wfm_stage_kind_t`` stops at
``STAGE_USER = 0x1000``; above it the kinds belong to the caller. In C a
caller allocates one and supplies its kernel through ``wfm_frame_ops_t``
(``native/examples/wfmgen_frame_demo.c`` §6). **Python gets no such hook,
deliberately** -- doppler#1125 closed on the reasoning that a per-stage
Python callback would make the one surface that must stay fast the one
surface that cannot be.

That is not the end of the road, and this file is the way round it:
**run the kernel here and hand the description a literal.** A Python
caller assembles the frame the built-in stages produce, applies its own
transform to those bits, and transmits the result. On receive it inverts
the transform and hands the *description* the recovered bits, which is the
same descriptor doing the same checking. The transform runs once per frame
in Python instead of inside the assembler, which is exactly the trade
doppler#1125 chose, and for a caller describing rather than streaming it
costs nothing.

The transform is a whitener -- XOR against a fixed pattern -- for two
reasons. It is what a real randomiser stage does, and it is its own
inverse, so the receive half costs no second kernel. ``STAGE_RANDOMISE``
is doppler's built-in one; the point here is a kind doppler does not have.

The asserts are physical, not structural: the whitened bits reach the
samples, the inverse recovers the frame bit for bit, the description's CRC
passes on the recovered bits, and -- the check that makes the rest a
demonstration rather than a coincidence -- it FAILS on the un-inverted
ones. A whitener that did nothing would pass every other assert here.

Run:
    python src/doppler/examples/frame_own_stage_demo.py
"""

from __future__ import annotations

import numpy as np

from doppler.wfm import STAGE_CRC16, STAGE_USER, Composer, FrameDesc, Segment

FS = 1.0e6  #: sample rate, Hz
SPS = 4  #: samples per symbol; rectangular, so a symbol is 4 copies

EMPTY = np.empty(0, np.uint8)

#: The kind this example allocates. Above STAGE_USER, so no version of
#: doppler will ever collide with it -- that guarantee is one-directional
#: and is the whole reason the kind is an open int rather than a menu.
MY_WHITEN = STAGE_USER + 1

#: An 8-bit period, short enough to check against the printed bits. A real
#: randomiser uses an LFSR; `STAGE_RANDOMISE` is doppler's.
PATTERN = np.array([1, 1, 0, 1, 0, 0, 1, 0], np.uint8)


def whiten(bits: np.ndarray) -> np.ndarray:
    """XOR ``bits`` against the repeating pattern -- its own inverse.

    This is the kernel. In C it would be a ``wfm_stage_op_t.in_unit``
    handed to the assembler through an ops table; here it is a function
    the caller runs itself, over the bits the description assembled.

    Parameters
    ----------
    bits
        One bit per byte, as every frame path in doppler passes them.

    Returns
    -------
    numpy.ndarray
        A new array; the input is not modified.
    """
    pat = np.resize(PATTERN, bits.size)
    return (bits ^ pat).astype(np.uint8)


def describe() -> FrameDesc:
    """A header, a payload, and a CRC over a span named rather than counted.

    The header sits deliberately OUTSIDE the CRC's cover: a receiver has to
    find the header before it can check anything. That choice is one
    argument to ``add_stage_over``, not an offset arithmetic exercise.
    """
    hdr = np.unpackbits(np.array([0x5C, 0x5C], np.uint8))
    payload = np.array([(i * 7 + 1) & 1 for i in range(24)], np.uint8)

    d = FrameDesc(EMPTY, EMPTY, EMPTY)
    assert d.add_field(hdr) == 0
    assert d.add_field(payload) == 1
    assert d.add_derived("crc", 16) == 2
    d.name_field(0, "hdr")
    d.name_field(1, "payload")
    # The cover REACHES the derived field, which is what wires that field's
    # producer -- so a CRC's position and the fact that a CRC produces it
    # are one declaration rather than two that can disagree.
    assert d.add_stage_over(STAGE_CRC16, "payload", "crc") == 0
    d.build()
    return d


def transmit(bits: np.ndarray) -> np.ndarray:
    """Clean rectangular BPSK, so a symbol is ``SPS`` identical samples."""
    seg = Segment(
        type="bits",
        fs=FS,
        sps=SPS,
        modulation="bpsk",
        snr=200.0,  # >= WFM_SYNTH_SNR_CLEAN: AWGN is skipped
        snr_mode="fs",
        seed=1,
        payload=bits.tobytes(),
        num_samples=bits.size * SPS,
    )
    return np.asarray(Composer([seg]).compose())


def demod(x: np.ndarray, nbits: int) -> np.ndarray:
    """Recover the bits a clean, rectangular, zero-offset stream carries.

    Legitimate only because the source is clean (no AWGN), rectangular (no
    filter delay to hunt for) and at zero frequency offset (no rotation):
    symbol ``i`` is then literally samples ``[i*SPS, (i+1)*SPS)`` and one of
    them is the whole story. ``bpsk_map`` is 0 -> +1, 1 -> -1, so the SIGN
    is the bit.
    """
    return (x[: nbits * SPS : SPS].real < 0).astype(np.uint8)


def main() -> int:
    print("=== a frame you described, a stage kind you own (Python) ===\n")

    # ── 1. The description ────────────────────────────────────────────────
    print("--- 1. Named fields, a stage over a named span ---")
    d = describe()
    frame = np.asarray(d.bits(1))
    print(f"  hdr      {d.field_bits(0):>3} bits @ {d.field_off(0):>3}")
    print(f"  payload  {d.field_bits(1):>3} bits @ {d.field_off(1):>3}")
    print(f"  crc      {d.field_bits(2):>3} bits @ {d.field_off(2):>3}")
    print(
        f"  stage 0  covers [{d.stage_first(0)}, "
        f"{d.stage_first(0) + d.stage_bits(0)})\n"
    )
    assert frame.size == d.nbits == 16 + 24 + 16
    assert d.check(frame).passed == 1, "the description checks its own frame"

    # ── 2. The kind is the caller's, and the description carries it ───────
    print("--- 2. A kind above everything doppler names ---")
    carrier = FrameDesc(EMPTY, EMPTY, EMPTY)
    carrier.add_field(frame)
    idx = carrier.add_stage(MY_WHITEN, first_field=0, n_fields=1)
    print(f"  add_stage(STAGE_USER + 1, ...) -> stage {idx}")
    assert idx == 0, "a description accepts a kind doppler never allocates"
    # ...and refuses to BUILD it, because no kernel here implements it.
    # That refusal is the design working: a stage that quietly did not run
    # produces a frame that still assembles, still decodes against itself,
    # and syncs to nothing. Python cannot supply the kernel (doppler#1125),
    # which is what section 3 works around rather than around which it
    # pretends.
    try:
        carrier.build()
        raise AssertionError("build() must refuse a kind it has no kernel for")
    except ValueError as exc:
        print(f"  build() -> refused: {str(exc).split(':')[0]}\n")

    # ── 3. Run the kernel here; hand the wire a literal ───────────────────
    print("--- 3. The kernel runs in Python, the description still checks ---")
    whitened = whiten(frame)
    assert not np.array_equal(whitened, frame), "the transform did something"

    x = transmit(whitened)
    rx = demod(x, whitened.size)
    print(f"  plain:    {''.join(map(str, frame.tolist()))}")
    print(f"  whitened: {''.join(map(str, whitened.tolist()))}")
    assert np.array_equal(rx, whitened), "the transform reached the samples"

    recovered = whiten(rx)  # its own inverse
    assert np.array_equal(recovered, frame), "the inverse recovers the frame"

    # The SAME description checks the recovered bits -- one descriptor, both
    # directions, with a transform it has never heard of in between.
    assert d.check(recovered).passed == 1, "the CRC passes on the recovered"

    # And the negative, which is what makes the rest a demonstration rather
    # than a coincidence: a whitener that did nothing would pass every
    # assert above and this one would still catch it.
    assert d.check(rx).passed == 0, "...and fails on the un-inverted bits"
    print("  recovered -> check passed;  un-inverted -> check failed\n")

    print("=== all checks passed ===")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
