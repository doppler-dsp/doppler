"""ring_lifecycle_demo.py — owning a ring, and owning a view of one.

A ring is a mapping, and a view is a window into it. Neither is an
ordinary Python object you can forget about, so the binding makes the two
lifetimes explicit -- and makes the easy spelling the correct one.

  THE RING    `with F32Buffer(n) as buf:` unmaps it on the way out.
              `destroy()` is the same thing by hand, and is safe to call
              twice. Afterwards every member raises rather than touching
              memory that is gone.

  THE VIEW    `wait(n)` / `peek(n)` LEND n samples: zero-copy, read-only,
              and the view keeps the ring alive (`view.base is buf`).
              `consume()` ends the loan. With no argument it releases
              exactly what was lent, so the count is written once; with a
              smaller one it releases a hop and keeps the overlap.

              After `consume()` the producer may overwrite those samples,
              so anything worth keeping is copied first.

Run:
  python ring_lifecycle_demo.py
"""

# --8<-- [start:setup]
import numpy as np

from doppler.buffer import F32Buffer

# --8<-- [end:setup]

# --8<-- [start:ring]
with F32Buffer(capacity=1024) as buf:
    # The size is rounded UP to what the mapping needs, so read it back.
    assert buf.capacity >= 1024
    buf.write(np.arange(8, dtype=np.complex64))

# Out of the block the mapping is gone, and the object says so.
try:
    _ = buf.available
    raise AssertionError("a destroyed ring answered")
except RuntimeError:
    pass
buf.destroy()  # again, by hand: harmless

# A size the ring cannot map is refused up front, as a ValueError.
try:
    F32Buffer(1000)  # not a power of two
    raise AssertionError("a ring of 1000 was mapped")
except ValueError:
    pass
# --8<-- [end:ring]

# --8<-- [start:view]
buf = F32Buffer(1024)
buf.write(np.arange(8, dtype=np.complex64))

view = buf.wait(4)
assert view.base is buf, "the view keeps the ring alive"
assert not view.flags.owndata, "zero-copy: these are the ring's bytes"
assert not view.flags.writeable, "a consumer reads what it was lent"

keep = view.copy()  # anything wanted after consume() is copied first
buf.consume()  # releases the 4 that were lent -- the count written once
assert buf.available == 4
assert np.array_equal(keep, [0, 1, 2, 3])

# Nothing is on loan now, so there is no count to default to.
try:
    buf.consume()
    raise AssertionError("consume() released a view nobody holds")
except RuntimeError:
    pass
# --8<-- [end:view]

# --8<-- [start:overlap]
# A hop smaller than the frame: release 1, keep 3, and the next frame
# overlaps the last by three samples.
starts = []
while (frame := buf.peek(4)) is not None:
    starts.append(int(frame[0].real))
    buf.consume(1)
assert starts == [4], "only one whole frame of 4 was left"

buf.write(np.arange(8, 16, dtype=np.complex64))
while (frame := buf.peek(4)) is not None:
    starts.append(int(frame[0].real))
    buf.consume(1)
assert starts == [4, 5, 6, 7, 8, 9, 10, 11, 12]
# --8<-- [end:overlap]

buf.destroy()
print(
    "lifecycle: with-block unmaps, destroyed members raise, views are "
    "read-only and pinned, consume() releases what was lent"
)
