- **`acq_get_state` writes every byte of its blob.** The blob reserves the
    whole ring and wrote only the buffered samples, so the rest carried
    whatever the caller's buffer held: blobs weren't reproducible, and a
    shipped blob carried stale heap. `DP_STATE_ROUNDTRIP_TEST` now checks
    every byte by serializing into two differently pre-filled buffers, and
    `make tests-ssot` requires every serializable object to use it. 30
    objects that predate the rule are ratcheted
    ([#1475](https://github.com/doppler-dsp/doppler/issues/1475)).
