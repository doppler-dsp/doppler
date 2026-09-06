"""The Python face of CCSDS 131.0-B's published literals.

The marker lived on ``doppler.wfm`` until doppler#1220 — a standard's name
in the general namespace, beside ``PN`` and ``Gold``, backed by a CCSDS
translation unit compiled into the general ``wfm_core``. It moved here; the
surface check moved with it, out of ``wfm``'s ``test_api_surface.py``.

Every assertion below compares against the **standard**, never against a
second expansion of ``0x1ACFFC1D`` written out here. That is the whole
reason the marker is a call and not a constant: an MSB-first expansion
written out twice is a transcription that can disagree with itself, and a
test that spells it out agrees with a receiver that spells it out the same
wrong way.
"""

from __future__ import annotations

import numpy as np

import doppler.ccsds as c


def test_all_is_exactly_the_published_surface() -> None:
    """The module carries the standard's DATA and nothing else.

    A guard rather than a tautology: `ccsds_tm`'s transforms — the outer
    code, the randomiser, the inner code — are reached by describing a CADU
    through `doppler.wfm.FrameDesc`, and this module growing a second door
    to them is exactly what docs/design/frame-description.md rules out.
    """
    assert c.__all__ == ["asm_bits"]


def test_asm_bits_is_the_marker() -> None:
    # 0x1ACFFC1D, first transmitted bit at the top of 0x1A (figure 9-1).
    # Compared as an integer rather than re-expanded bit by bit, so this is
    # a check against the standard and not against the code under test.
    b = np.asarray(c.asm_bits())
    assert b.dtype == np.uint8 and b.size == 32
    assert int("".join(map(str, b.tolist())), 2) == 0x1ACFFC1D


def test_asm_bits_returns_a_fresh_array_each_call() -> None:
    """Two calls do not share a buffer.

    `SyncFinder(asm_bits())` passes a temporary, and a caller that keeps one
    result while asking for another must not see it change underneath.
    """
    a, b = c.asm_bits(), c.asm_bits()
    assert a is not b
    assert np.array_equal(a, b)
    a[0] ^= 1
    assert not np.array_equal(a, b)


def test_marker_is_what_the_general_searcher_acquires_on() -> None:
    """The one composition that matters: this module supplies the pattern,
    `detection` supplies the search, and neither knows about the other's
    subject. `SyncFinder` is general — it takes a marker — and CCSDS
    reaches it from here.
    """
    from doppler.detection import SyncFinder

    marker = np.asarray(c.asm_bits())
    rng = np.random.default_rng(1220)
    stream = rng.integers(0, 2, 500, dtype=np.uint8)
    at = 173
    stream[at : at + marker.size] = marker

    hit = SyncFinder(marker).find(stream, max_errors=0)
    assert hit is not None
    assert hit.offset == at
