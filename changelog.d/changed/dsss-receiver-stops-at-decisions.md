- **`DsssBurstReceiver` and `BurstDemod` stop at hard and soft decisions**
    ([#1022](https://github.com/doppler-dsp/doppler/issues/1022)). They took a
    frame's shape — first as a hard-coded `sync | payload | CRC-16`, then as a
    description with four knobs and a `frame_valid` verdict — and neither is a
    physical-layer fact. They now take a sync word to correlate and
    `frame_syms` to slice; `push()` returns the frame's bits, `llrs()` the same
    decisions as soft values, and that is the whole output. `payload_len` is
    gone, `set_sync()` replaces `set_frame()`, and the `ccsds_tm`/`conv`/`rs`
    link line goes with them.
