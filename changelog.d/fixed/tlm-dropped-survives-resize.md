- **A telemetry capture reports exactly the drops after it opened, even when
    the ring grows.** `dp_tlm_resize` now carries the drop count into the
    ring it swaps in, so `dp_tlm_dropped` is monotonic for the life of the
    context. A late probe that grew the ring mid-capture used to restart the
    count, so close either wrapped to about 2^64 and failed a clean capture,
    or lost the drops before the resize and passed one with a hole (#2649).
