"""buffers_demo.py — three rings, one shape: start here.

`doppler.buffer` has one ring in three widths. They differ in the element
and in nothing else, so everything below is written once and run on all
three:

    F32Buffer   complex64                      8 bytes / sample
    F64Buffer   complex128                    16 bytes / sample
    I16Buffer   [("i", "<i2"), ("q", "<i2")]   4 bytes / sample

A ring hands samples from one producer to one consumer. `write()` copies
a block in; `wait(n)` LENDS the consumer a view of the next n samples --
zero-copy, and contiguous even when those n straddle the physical end of
the ring, which is what the double mapping is for; `consume()` gives them
back. Every count is in SAMPLES, on every width.

The rest of the suite takes one subject each:

    ring_lifecycle_demo     owning a ring, and owning a view of one
    ring_chunking_demo      the producer's block size is not the consumer's
    ring_nonblocking_demo   one thread: peek / write_some / space / reset
    ring_iq16_demo          16-bit I/Q as a record, without a copy
    ring_refusals_demo      everything a ring says no to
    ring_interrupt_demo     stopping a consumer blocked in wait()

Run:
  python buffers_demo.py
"""

# --8<-- [start:setup]
import threading

import numpy as np

from doppler.buffer import F32Buffer, F64Buffer, I16Buffer

IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])


# A ramp in each width's own element, so a sample says where it came from.
def ramp_complex(dtype):
    return lambda n: np.arange(n).astype(dtype)


def ramp_iq16(n):
    x = np.zeros(n, dtype=IQ16)
    x["i"] = np.arange(n)
    return x


# --8<-- [end:setup]

# --8<-- [start:widths]
WIDTHS = [
    (F32Buffer, np.dtype(np.complex64), ramp_complex(np.complex64)),
    (F64Buffer, np.dtype(np.complex128), ramp_complex(np.complex128)),
    (I16Buffer, IQ16, ramp_iq16),
]

for cls, dtype, ramp in WIDTHS:
    with cls(1024) as buf:
        # Rounded UP to what the mapping needs: read it back, never assume.
        cap = buf.capacity
        assert cap >= 1024 and cap & (cap - 1) == 0

        assert buf.write(ramp(100))
        assert (buf.available, buf.space) == (100, cap - 100)

        view = buf.wait(64)
        assert view.shape == (64,), "1-D, one element per sample"
        assert view.dtype == dtype
        assert np.array_equal(view, ramp(64))
        buf.consume()
        assert buf.available == 36
# --8<-- [end:widths]

# --8<-- [start:wrap]
# The point of the double mapping: a frame that straddles the end of the
# ring still comes back as ONE contiguous array. Advance to 100 samples
# short of the end, then ask for 256.
with F32Buffer(1024) as buf:
    cap = buf.capacity
    buf.write(np.zeros(cap - 100, dtype=np.complex64))
    buf.wait(cap - 100)
    buf.consume()

    buf.write(np.arange(256, dtype=np.complex64))  # wraps after 100
    frame = buf.wait(256)
    assert frame.flags["C_CONTIGUOUS"]
    assert np.array_equal(frame, np.arange(256))
    buf.consume()
# --8<-- [end:wrap]

# --8<-- [start:threads]
# Two threads. wait() releases the GIL while it spins, so the producer
# runs; close() is how the consumer learns there is no more.
buf = F64Buffer(4096)
got = []


def producer():
    for k in range(8):
        while buf.space < 512:  # write() never blocks: wait for room
            pass
        buf.write(np.full(512, k, dtype=np.complex128))
    buf.close()


t = threading.Thread(target=producer)
t.start()
try:
    while True:
        got.append(int(buf.wait(512)[0].real))
        buf.consume()
except EOFError:
    pass  # closed and drained: the normal end of a stream
t.join()

assert got == list(range(8)), "every block, once, in order"
assert buf.dropped == 0
buf.destroy()
# --8<-- [end:threads]

print(
    "buffers: three widths share one shape; a frame across the wrap is "
    f"contiguous; {len(got)} blocks crossed two threads in order"
)
