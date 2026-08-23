- **`dp_stream_interrupt()` — a blocking receive you can actually stop.**
    A blocking `*_recv` waits inside the NATS client, and the flag a signal
    handler sets is read by a loop the blocking call is keeping you out of.
    With traffic arriving that is invisible; the moment a sender stops,
    Ctrl+C stops working. doppler's own C receiver example shipped that way.

    The interrupt is a process-wide flag the library checks *inside* the
    wait, so a blocking receive stays blocking and still returns — with
    `DP_ERR_INTERRUPTED`, a request to stop rather than a failure. It
    assigns to a `volatile sig_atomic_t` and does nothing else, so a signal
    handler may call it; `dp_stream_interrupt_on_signal()` installs such a
    handler and **chains** to whatever was there, and `dp_stream_resume()`
    clears the flag. Every wait is sliced at 100 ms, and the flag is checked
    before the first slice, so interrupt-then-receive and
    receive-then-interrupt behave the same.

    In Python: `interrupt_on_sigint()`, a context manager, plus
    `interrupt()` / `resume()` / `interrupted()` for a worker thread. An
    unblocked `recv()` raises `KeyboardInterrupt`, so existing
    `try`/`finally` and `with` cleanup applies unchanged.

    Two things had to be measured rather than reasoned. The handler cannot
    be a Python one — it runs when the interpreter regains control, which
    is exactly what the blocking wait prevents, and the first version of
    `interrupt_on_sigint()` used `signal.signal` and left a blocked `recv()`
    blocked forever. And the exception is raised **once**: the binding calls
    `PyErr_CheckSignals()` first and lets CPython raise its own pending one,
    because raising ours too delivered a second `KeyboardInterrupt` into the
    caller's cleanup block.

    Both receiver examples now block with no timeout and stop instantly,
    which is the shape a dashboard wants and could not have before.
