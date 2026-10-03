- **The C burst-receiver tests, benches and example describe their frame.**
    `test_burst_demod_core`, `test_dsss_burst_receiver_core`, their benches and
    `dsss_burst_receiver_demo` build a `wfm_frame_desc_t` and the
    `*_create_desc` constructors in place of a hand-counted `frame_syms` and
    `set_sync()`, and build their bursts with `dp_wfm_dsss_desc_chips` instead
    of a private builder (proved sample-identical before it was deleted)
    ([#1620](https://github.com/doppler-dsp/doppler/issues/1620)).
