- **The `frame_syms` constructors of `BurstDemod` and `DsssBurstReceiver` are
    gone.** C: `dp_burst_demod_create()`, `dp_burst_demod_set_sync()` and
    `dp_dsss_burst_receiver_create()` (a sync word plus a hand-counted
    `frame_syms`) are removed, with the receiver's hand-rolled CRC check;
    build from a frame description with `dp_burst_demod_create_desc()` /
    `dp_dsss_burst_receiver_create_desc()`. The receiver state loses `sync`,
    `sync_len` and `frame_bits` (use `frame_syms`, now read from the
    description). The serialized blob is unchanged
    ([#1620](https://github.com/doppler-dsp/doppler/issues/1620)).
