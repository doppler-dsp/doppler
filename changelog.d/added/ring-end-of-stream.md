- **The ring buffer can say it is finished, and its wait can end.**
    `dp_<t>_close()` marks a ring closed; `dp_<t>_closed()` reads that back;
    and `dp_<t>_wait()` now returns `NULL` instead of spinning forever, for
    two reasons the caller tells apart: the producer closed and the ring is
    drained, or somebody interrupted the process.

    This was the worst of the three defects
    [`io-termination.md`](https://github.com/doppler-dsp/doppler/blob/main/docs/design/io-termination.md)
    catalogued, and the only one with **no escape at all**: `wait()` was an
    unbounded `DP_SPIN_HINT()` loop with no timeout, no flag check and no
    producer-done concept, so a producer that stopped left the consumer
    spinning at 100% CPU — and because the loop read nothing, no signal
    handler could rescue it.

    The interrupt escape is pinned by a test that fails **by hanging**,
    which is how the bug itself presented: remove the check and
    `test_buffer_core` times out instead of failing. The end-of-stream cases
    are pinned four ways, including a threaded one where a consumer already
    spinning receives the producer's final batch.

    `wait()` may now return `NULL`, which is a contract change — its one
    caller, `bench_buffer_core`, guards it.
