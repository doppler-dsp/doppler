- **`wfmgen --continuous` could not be stopped without losing its output**
    (doppler#969). It advertises `--continuous --realtime --output nats://…`
    in its own `--help` and had **no signal handling at all** — no
    `signal()`, no `SIGINT`, nothing, in 1500 lines. Ctrl+C killed it
    outright.

    The two destinations failed differently, and the file one is worse than
    it sounds: the BLUE header carries the final sample count and is written
    by `wfm_writer_close`, so a killed process left a capture with **no
    valid header**. A short capture reads; a headerless one does not. On the
    NATS side a send returns once the *client* has the block, not the
    server, so exiting left the tail to the client's own best-effort flush —
    500 ms, no failure report, silently dropped past that.

    Fixed by installing the handler as the first thing `doppler_wfmgen()`
    does (before parsing, before opening anything — a signal arriving
    earlier terminates the process regardless), checking the flag in both
    emit loops, and draining the stream sink with a reported result on every
    exit path. `wfm_stream_sink_drain()` is the new verb; a failed drain now
    makes the exit code non-zero, so "wfmgen exited 0" means the samples
    arrived.

    **A fourth instance of the same defect turned up in the fix.**
    `--realtime` pacing sleeps in `sleep_until_mono_ns`, which retried on
    `EINTR` by design — drift-free pacing, and a wait no signal could ever
    end. Paced against a low sample rate that sleep is seconds long, so the
    interrupt was swallowed entirely: measured, `--continuous --realtime`
    never exited, while the same run without `--realtime` exited in 3 ms.
    The sleep is now **sliced**, with the flag checked between slices, at
    the same `dp_interrupt_latency_ms()` cadence the NATS wait uses. The
    first attempt only checked where `clock_nanosleep` returns `EINTR`,
    which works when a signal happens to land and not at all when a caller
    simply sets the flag — no signal, no `EINTR`, no check. It also has
    `SA_RESTART` against it: Linux restarts an absolute `clock_nanosleep`
    by itself under that flag. Slicing makes the bound a property of the
    code rather than of whether a signal arrived. Same shape as the ring's
    spin and the blocking recv, in a third place —
    `docs/design/io-termination.md` predicted the class, not this instance.

    After: 3 ms to exit, 3/3, exit 0, capture readable, on both paths.

    Gated by `src/doppler/wfm/tests/test_wfmgen_shutdown.py`, sabotage-checked
    on **both** halves: removing the handler fails in 0.58 s (killed by the
    signal), removing the interruptible sleep fails in 40.68 s (hangs to the
    deadline).

    `StreamSink.drain(timeout_ms=0)` puts the same verb on the Python face,
    declared (`returns = "int"` + `error`) rather than hand-written. Note
    the spelling: a **handle** method needs `returns = "int"` and refuses
    the `status_return` an **object** method takes — and having taken it,
    silently drops `error_message` and emits no `Raises` section, both of
    which an object method gets. Filed as just-makeit#1111.
