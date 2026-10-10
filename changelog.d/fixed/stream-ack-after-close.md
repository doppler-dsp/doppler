- **An ack after its `Pull` is closed is refused, not a use-after-free.**
    `dp_msg_ack()` after the context that received the message was
    destroyed read the freed subscription; it now returns `DP_ERR_CLOSED`
    and the broker redelivers the frame. Python's `Pull.ack()` raises
    `ValueError` then, also after the `Pull` is garbage-collected, and
    `Pull.close()` no longer holds the GIL while it waits (#2016).
