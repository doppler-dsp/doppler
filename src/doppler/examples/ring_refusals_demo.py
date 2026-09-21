"""ring_refusals_demo.py — everything a ring says no to, and how it says it.

A ring sits on a hot path, so it does not guess. Each thing it will not do
is a distinct exception, raised before anything is copied, with a message
that carries the number or the type that was wrong. This walks all of
them, because the useful part of an error is knowing which one to catch.

  TypeError     the array is not the ring's element type
  ValueError    the array is the wrong shape or not contiguous; or the
                request can never be satisfied (n > capacity, a size the
                ring cannot map); or consume(n) asks to release more than
                is there
  EOFError      the producer closed the ring and fewer than n remain --
                the NORMAL end of a stream, which a consumer loop catches
  RuntimeError  the ring is destroyed; or consume() with nothing on loan
  False / 0     the ring is full. Not an exception: write() refuses the
                block and returns False, write_some() takes what fits.
  None          peek() only: not yet. Never anything else.

Run:
  python ring_refusals_demo.py
"""

# --8<-- [start:setup]
import threading

import numpy as np

from doppler.buffer import F32Buffer


def refusal(exc, call):
    """The message `call` raises, which must be an `exc`."""
    try:
        call()
    except exc as e:
        return str(e)
    raise AssertionError(f"expected {exc.__name__}")


buf = F32Buffer(1024)
cap = buf.capacity
# --8<-- [end:setup]

# --8<-- [start:inputs]
# A ring exists to avoid copies, so it will not quietly make one: an input
# that would need casting, flattening or compacting is refused instead.
msg = refusal(TypeError, lambda: buf.write(np.zeros(8, dtype=np.float32)))
assert "float32" in msg, "the message says what it was given"

grid = np.zeros((8, 2), dtype=np.complex64)
refusal(ValueError, lambda: buf.write(grid))  # 2-D
refusal(ValueError, lambda: buf.write(grid[:, 0]))  # strided
assert buf.available == 0 and buf.dropped == 0, "nothing was taken"
# --8<-- [end:inputs]

# --8<-- [start:full]
# Full is not an error. write() is all-or-nothing and says False;
# write_some() takes what fits and says how much.
assert buf.write(np.zeros(cap, dtype=np.complex64)) is True
assert buf.write(np.zeros(1, dtype=np.complex64)) is False
assert buf.dropped == 1, "counts what was REFUSED -- the caller still has it"
assert buf.write_some(np.zeros(8, dtype=np.complex64)) == 0
assert buf.dropped == 1, "write_some never refuses, so never counts"
buf.reset()
# --8<-- [end:full]

# --8<-- [start:never]
# A request no producer could ever satisfy is a caller bug, and is said
# plainly -- with both numbers -- rather than waited on forever.
msg = refusal(ValueError, lambda: buf.wait(cap + 1))
assert str(cap + 1) in msg and str(cap) in msg
refusal(ValueError, lambda: buf.peek(cap + 1))
# --8<-- [end:never]

# --8<-- [start:release]
# Releasing more than is there is refused too, and releases NOTHING. The
# ring's two positions are all it knows about itself: let the read position
# pass the write position and every later count would describe a ring that
# does not exist.
buf.write(np.zeros(10, dtype=np.complex64))
msg = refusal(ValueError, lambda: buf.consume(11))
assert buf.available == 10 and buf.space == cap - 10, "still a ring"
buf.consume(10)
# --8<-- [end:release]

# --8<-- [start:eos]
# "Not yet" and "never" are different answers. peek() says None for the
# first and raises EOFError for the second, so a poll loop cannot mistake
# a finished stream for a slow one.
buf.write(np.zeros(100, dtype=np.complex64))
assert buf.peek(512) is None  # not yet
buf.close()
refusal(EOFError, lambda: buf.peek(512))  # never
refusal(EOFError, lambda: buf.wait(512))  # and wait() does not spin
assert len(buf.peek(100)) == 100, "what IS there is still readable"
buf.consume()
# --8<-- [end:eos]

# --8<-- [start:threads]
# The pattern that keeps a consumer from waiting forever: the producer
# closes the ring in `finally`. If it raises -- here, by writing the wrong
# type -- the consumer's wait() ends in EOFError instead of never.
buf.reset()
failed = []


def producer():
    try:
        buf.write(np.zeros(256, dtype=np.float32))  # wrong type: raises
    except TypeError as e:
        failed.append(e)
    finally:
        buf.close()


t = threading.Thread(target=producer)
t.start()
refusal(EOFError, lambda: buf.wait(256))
t.join()
assert len(failed) == 1
# --8<-- [end:threads]

buf.destroy()
refusal(RuntimeError, lambda: buf.peek(1))
print(
    "refusals: TypeError / ValueError for inputs, False / 0 for full, "
    "None for not yet, EOFError for the end, RuntimeError after destroy"
)
