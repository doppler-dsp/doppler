- **`Push.send_eos()`, and an example for the tier it belongs to.** The
    Python face could announce an ending on `Publisher` but not on `Push`,
    while the docs described the work-queue tier's guarantees on both — so
    the one tier whose ending is *reliable* was the one that could not send
    it from Python.

    `src/doppler/examples/work_queue_shutdown_demo.py` is the SPMC case: one
    producer, five consumers, and three claims it asserts rather than
    describes.

    - **Nothing is lost.** A work queue is at-least-once, unlike PUB/SUB —
        every frame and the ending arrive.
    - **Exactly one consumer hears it.** A work queue load-balances, and the
        ending is a message like any other, so `send_eos()` ends *one* of the
        five and the other four wait forever. This is the trap: it looks like
        a broadcast shutdown and is not one. 1:5 is where that stops being
        subtle — ending a pool of N needs N markers, or a different tier.
    - **The ending does not outlive the run** (see the ack fix above).

    The even frame split the demo prints is its own polling rotation, not the
    broker's, and it says so: the count that carries the claim is the total —
    twenty frames between five consumers rather than twenty each.
