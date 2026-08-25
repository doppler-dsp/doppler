- **`DsssBurstReceiver.push()` no longer discards its input, and returns
    **every** burst it completed**
    ([#1008](https://github.com/doppler-dsp/doppler/issues/1008)). A block
    carrying several bursts lost all but the first — measured on the example
    capture at **6/6 decoded with 333-sample blocks against 1/6 with one
    large one**. It now decodes **5/5 at every block size**, including the
    entire 1.48 M-sample capture in a single call, with `dropped == 0`.

    **Three separate discard sites**, all removed:

    - an early return that, when a burst was already ready, never looked at
        `x` at all — the whole block gone;
    - a `break` in the chunk loop that left `x[off..x_len)` unwritten to the
        history ring and unseen by acquisition, and uncounted in `dropped`;
    - a single `burst_acq_push` per chunk. `acq_push` stops once its result
        array is full and abandons its own input suffix, so acquisition never
        framed the rest of a chunk this object was already holding.

    **The API change: `push()` returns the payloads of every burst that
    completed**, concatenated, with a new **`events()`** giving each one its
    own record. One call can complete many bursts and the scalar read-backs
    can only ever describe the last, so each payload needs its own event —
    the sufficiency argument in `docs/design/dsss-burst-receiver.md` §4 is
    about what one event must contain, not how many are returned per call.
    `push_max_out(x_len)` now uses the `x_len` it always accepted and
    ignored: distinct bursts cannot overlap, so the count is bounded by
    `x_len/burst_len + 1` plus the queue depth.

    **Consuming the whole input needed the retention bound restored.**
    Removing the `break` alone makes the ring refuse samples
    (`dropped=5632`): `chunk_max` only guarantees a write fits if `trim` can
    release down to `retain_span`, and `trim` clamps to the oldest queued
    detection. Draining **every** detection whose window has arrived — rather
    than one per chunk — means the oldest remaining one always has an
    unarrived window, so occupancy stays under `retain_span` and `dropped` is
    0 by construction. `DSSS_BR_QCAP = 8` is retired for a derived `q_cap`:
    entries sit ≥ `refine_span` apart within `retain_span`, so the count
    scales with `burst_len/refine_span` — ~1 at the C tests' geometry but
    **5.5× at a real link**, where the fixed cap silently dropped a hit *and
    the rest of its batch*.

    **Re-feeding acquisition needed one correction to the repo idiom**, and
    getting it wrong would have been worse than the bug. Composers recover a
    child's tail by diffing `samples_fed − child->samples_consumed`
    (`dsss_receiver_core.c:392-427`), which is right when the tail goes to a
    *different* stage. Here it goes back to the *same* acq, and
    `samples_consumed` counts only **framed** samples — diffing against it
    would re-feed what acq holds unframed in its ring, a **double-fed
    stream**. The invariant quantity is framed plus ring-resident.

    **Nothing caught this because no test anywhere put two bursts in one
    stream.** Every helper in the C suite and in `validate.py` placed exactly
    one, so everything `push()` discarded after it was noise and the loss was
    unobservable; the certification's own `any_block_size` limit had the same
    blind spot. The suite gains a multi-burst capture builder, a block-size
    invariance test and an acquisition-saturation test, and the certification
    gains §2.11 and **F11** — now **32 limits, 11 findings, 0 open**.

    `DSSS_BURST_RECEIVER_STATE_VERSION` is **3**: the detection queue is
    heap-allocated at the derived capacity, so the blob layout moved. The
    event list is deliberately *not* serialized — it describes one call, and
    keeping it out is what holds `state_bytes()` to a pure function of
    configuration (finding F5).
