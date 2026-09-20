"""ring_nonblocking_demo.py — one thread, no deadlock: the ring's
non-blocking surface.

`wait(n)` spins until a producer on ANOTHER thread delivers. A caller that
is its own producer -- a read loop, a callback, a notebook cell -- would
wait forever for samples only it can write. The non-blocking surface is
the same ring for that caller:

  peek(n)        the view `wait(n)` would return, or None for "not yet"
  write_some(a)  takes what fits and says how much; never refuses
  space          the room a write is guaranteed to find
  reset()        empty AND reopen, so one mapping carries a second stream

Three parts, each the Python twin of a shape that occurs:

  A. DRIP-FEED — arrivals smaller than a frame; ask "is there a frame
     yet?" after each one.
  B. A CHUNK LARGER THAN THE RING, read as OVERLAPPED frames. `write()` is
     all-or-nothing, so it can never accept this chunk; `write_some` feeds
     it by looping. `consume(HOP)` with HOP < NFFT is how frames overlap,
     and NFFT does not divide the capacity, so frames straddle the
     physical end of the ring and come back contiguous anyway.
  C. END OF STREAM, THEN REUSE — a closed ring raises EOFError for a frame
     that can no longer arrive, which `None` must never mean; `reset()`
     reopens the same ring.

The stream is a ramp, so every sample says where it came from, and every
frame is checked against it sample for sample.

Run:
  python ring_nonblocking_demo.py
"""

# --8<-- [start:setup]
import numpy as np

from doppler.buffer import F32Buffer


def stream(start: int, n: int) -> np.ndarray:
    idx = np.arange(start, start + n)
    return (idx + 1j * idx).astype(np.complex64)


ring = F32Buffer(4096)
CAP = ring.capacity

# --8<-- [end:setup]
# ── A. drip-feed until a frame is there, then take it ───────────────────

# --8<-- [start:drip]
PIECE, FRAME, ARRIVALS = 100, 1024, 25
sent = frames = 0
for _ in range(ARRIVALS):
    assert ring.write_some(stream(sent, PIECE)) == PIECE
    sent += PIECE
    frame = ring.peek(FRAME)  # None until FRAME samples have accumulated
    if frame is not None:
        assert np.array_equal(frame, stream(frames * FRAME, FRAME))
        ring.consume(FRAME)
        frames += 1

# --8<-- [end:drip]
assert frames == sent // FRAME, "a frame was missed or taken early"
assert ring.available == sent % FRAME
print(
    f"A. {ARRIVALS} arrivals of {PIECE} -> {frames} frames of {FRAME}, "
    f"{ring.available} left over"
)

# ── B. a chunk larger than the ring, read as overlapped frames ──────────

# --8<-- [start:chunk]
ring.reset()
NFFT, HOP = 1000, 250
assert CAP % NFFT, "frames must straddle the wrap for this to show anything"
chunk = stream(0, 5 * CAP)
assert not ring.write(chunk[: CAP + 1]), "write() refuses what cannot fit"
refused = ring.dropped

fed = taken = 0
while fed < len(chunk):
    k = ring.write_some(chunk[fed:])
    fed += k
    drained = 0
    while (frame := ring.peek(NFFT)) is not None:
        assert frame.flags["C_CONTIGUOUS"], "the double-mapping's promise"
        assert np.array_equal(frame, chunk[taken * HOP :][:NFFT]), (
            f"frame {taken} is not the stream at hop {taken * HOP}"
        )
        ring.consume(HOP)  # release a hop, keep the overlap
        taken += 1
        drained += 1
    # A full ring holds a whole frame, so one side always moves. Without
    # this, a regression in either call is a hang rather than a failure.
    assert k or drained, "neither side made progress"

# --8<-- [end:chunk]
assert ring.dropped == refused, "write_some never counts a drop"
assert taken == (len(chunk) - NFFT) // HOP + 1, "every overlapped frame"
assert taken * HOP > 3 * CAP, "the run must cover several wraps"
print(
    f"B. one {len(chunk):,}-sample chunk through a {CAP:,}-sample ring -> "
    f"{taken} frames of {NFFT} at hop {HOP}, all identical to the input"
)

# ── C. end of stream, then the same ring again ──────────────────────────

# --8<-- [start:eos]
ring.close()
tail = ring.available
assert 0 < tail < NFFT
try:
    ring.peek(NFFT)
    raise AssertionError("a closed ring answered 'not yet'")
except EOFError:
    pass  # the rest of that frame is never coming: not None, an error
last = ring.peek(tail)  # what IS there can still be read
assert np.array_equal(last, chunk[-tail:])
ring.consume()

ring.reset()
assert not ring.closed and ring.available == 0 and ring.space == CAP
assert ring.write_some(stream(0, 8)) == 8
assert np.array_equal(ring.peek(8), stream(0, 8))
# --8<-- [end:eos]
print(
    f"C. closed with {tail} left: peek({NFFT}) raised EOFError, "
    f"peek({tail}) read the tail; reset() reopened the ring"
)
