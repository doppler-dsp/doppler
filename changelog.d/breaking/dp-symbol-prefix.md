- **Every C symbol just-makeit derives now carries `dp_`**
    ([#1545](https://github.com/doppler-dsp/doppler/issues/1545)):
    `fir_create` is `dp_fir_create`, `fir_state_t` is `dp_fir_state_t`. A new
    ABI; rebuild against the new headers. Python names do not change; the
    Rust crate's raw `extern "C"` declarations follow C. Names doppler spelled
    itself (`wfm_*`, vendored cJSON/nats.c) are not prefixed yet, and a new
    bare export fails `make symbol-prefix-check`
    ([#1565](https://github.com/doppler-dsp/doppler/issues/1565)).
- **`wfm_frame_desc_layout_t.frame_bits` is now `frame_nbits`** (and the
    `dsss_burst_receiver` state field): the old name collides with the derived
    `dp_frame_bits`.
