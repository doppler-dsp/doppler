- **A multi-frame data source, sent over DSSS and received** —
    `data_source_link_demo.py` (and its C twin) sends a message one chunk per
    burst, builds `DsssBurstReceiver` from the same `FrameDesc`, and gets the
    message back byte for byte through `FrameDesc.check`/`deframe`. The
    BPSK data-source demo now reads its frames back the same way instead of
    slicing bit positions by hand.
    ([#1620](https://github.com/doppler-dsp/doppler/issues/1620))
