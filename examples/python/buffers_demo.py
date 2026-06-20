"""buffers_demo.py — smoke test for ring buffer examples.

From docs/examples/python-buffers.md.
"""

import threading

import numpy as np

from doppler.buffer import F32Buffer, F64Buffer, I16Buffer

# F64 producer / consumer with thread joins
buf = F64Buffer(256)
received = []


def producer():
    data = (np.ones(256) + 2j * np.ones(256)).astype(np.complex128)
    ok = buf.write(data)
    assert ok, "write should succeed into an empty buffer"


def consumer():
    view = buf.wait(256)
    received.append(view[:2].copy())
    buf.consume(256)


t_c = threading.Thread(target=consumer)
t_p = threading.Thread(target=producer)
t_c.start()
t_p.start()
t_p.join()
t_c.join()

assert len(received) == 1
assert np.allclose(received[0], [1 + 2j, 1 + 2j]), (
    f"unexpected data: {received[0]}"
)
assert buf.dropped == 0

# Overflow detection
buf2 = F64Buffer(256)
buf2.write(np.zeros(256, dtype=np.complex128))
buf2.write(
    np.zeros(256, dtype=np.complex128)
)  # second write hits a full buffer
assert buf2.dropped == 256, f"expected 256 dropped, got {buf2.dropped}"

# F32 and I16 types exist and report correct capacity
buf32 = F32Buffer(512)
assert buf32.capacity == 512

buf16 = I16Buffer(1024)
assert buf16.capacity == 1024

print("buffers_demo: OK")
