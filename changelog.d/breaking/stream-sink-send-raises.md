- **`StreamSink.send()` raises `OSError` on a refused block and returns
    `None`, and the C send calls must be checked** (#2011). A block is refused
    when the client is reconnecting with 8 MiB already pending, or the sink
    was drained. `send()` used to return the C status as an `int`; delete any
    test of its return value (`if sink.send(b) != 0:` now always reads as a
    failure) and catch `OSError`. In C, the `dp_pub_*`, `dp_push_*`,
    `dp_req_*` and `dp_rep_*` sends, `dp_pub_flush`, `dp_pub_send_eos`,
    `dp_stream_drain` and `dp_wfm_stream_sink_send`/`_send_eos`/`_drain`
    are `DP_NODISCARD`: a call that drops the status warns, and fails under
    `-Werror`. Check it; GCC does not accept a `(void)` cast.
