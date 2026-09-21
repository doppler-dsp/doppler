"""ring_interrupt_demo.py — stopping a consumer that is blocked in wait().

`wait(n)` releases the GIL and spins in C until n samples arrive. That is
what lets a producer thread run -- and it is also why an ordinary Python
flag cannot stop it: nothing in that loop is running Python.

`doppler.interrupt.Interrupt` is the stop the C side listens to. The flag
it sets is PROCESS-WIDE, shared by every doppler module, so a guard made
anywhere reaches a wait blocked anywhere. When it fires, `wait()` raises
`KeyboardInterrupt` -- the same thing Ctrl-C raises, because
`Interrupt([signal.SIGINT])` is how Ctrl-C is wired to it.

Two endings, and a consumer loop usually wants both:

    EOFError            the producer finished (it called close())
    KeyboardInterrupt   somebody asked the process to stop

Without a guard, a wait with no producer spins forever and Ctrl-C does not
reach it. So: a guard around anything that waits.

Run:
  python ring_interrupt_demo.py
"""

# --8<-- [start:setup]
import threading
import time

import numpy as np

from doppler.buffer import F32Buffer
from doppler.interrupt import Interrupt

buf = F32Buffer(1024)
# --8<-- [end:setup]

# --8<-- [start:stop]
# `[]` installs no signal handlers: this guard is only a handle to the
# flag. `Interrupt([signal.SIGINT])` is the same with Ctrl-C attached.
with Interrupt([]) as stop:
    # Nothing will ever write 512 samples, so this wait would never return.
    # Another thread asks for a stop a fifth of a second in.
    threading.Timer(0.2, stop.interrupt).start()

    t0 = time.monotonic()
    try:
        buf.wait(512)
        raise AssertionError("wait() returned with nothing written")
    except KeyboardInterrupt:
        waited = time.monotonic() - t0
    assert 0.1 < waited < 5.0, f"stopped after {waited:.2f} s"
    assert stop.interrupted()
    # --8<-- [end:stop]

    # --8<-- [start:resume]
    # The flag stays set until it is cleared, so a second wait would stop
    # at once. resume() clears it, and the ring is as it was.
    stop.resume()
    assert not stop.interrupted()
    buf.write(np.arange(4, dtype=np.complex64))
    assert len(buf.wait(4)) == 4
    buf.consume()
    # --8<-- [end:resume]

# --8<-- [start:loop]
# The consumer loop with both endings. close() ends it here; a stop would
# end it the same way, through the other except.
frames = 0
stopped = False


def producer():
    for _ in range(5):
        while buf.space < 256:
            pass
        buf.write(np.zeros(256, dtype=np.complex64))
    buf.close()


with Interrupt([]):
    t = threading.Thread(target=producer)
    t.start()
    try:
        while True:
            buf.wait(256)
            frames += 1
            buf.consume()
    except EOFError:
        pass  # the producer finished
    except KeyboardInterrupt:
        stopped = True  # somebody asked the process to stop
    t.join()
assert frames == 5 and not stopped, "ended by close(), not by a stop"
# --8<-- [end:loop]

buf.destroy()
print(
    f"interrupt: a wait with no producer stopped in {waited:.2f} s; "
    f"a guarded loop took {frames} frames and ended on close()"
)
