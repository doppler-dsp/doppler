- **A worked example of stopping a stream cleanly**
    (`src/doppler/examples/graceful_shutdown_demo.py`), driving the real
    `wfmgen` rather than a toy. It asserts the three things that were
    previously unanswerable — the run can be interrupted, the consumer
    *learns* the stream ended (`EOFError`, not a timeout that means only
    "nothing yet"), and the tail actually arrived (exit 0 is the drain's
    verdict).

    It runs the producer **unpaced**, because that is the condition the
    interrupt bound had never been measured on: idle, a wait checks the flag
    ten times a second; saturated, it is inside the transport's wait nearly
    always. Measured on one machine: **the producer stops in ~31 ms while
    saturated at ~3 GB/s**, which is the first evidence the bound holds at
    rate.

    `wfmgen` also gained the announcement it was missing — it drained but
    never said it had finished, so a consumer still had only silence to
    interpret. `wfm_stream_sink_send_eos()` is now sent before the drain,
    which is the order the header prescribes.

    Two hazards found while writing it, both recorded in the example:
    reading only *after* the interrupt makes the client buffer the whole run
    (a 5 s unpaced run queued **~16 GB**, which would OOM a CI runner) and
    then "receives" it at 18 GB/s, because doppler's recv is zero-copy and
    was handing out pointers to RAM rather than reading a socket — a rate
    for a transfer that had already happened. Consuming concurrently bounds
    the memory and makes producer and consumer measurable over the same
    interval.
