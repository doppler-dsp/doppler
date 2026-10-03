- **`DsssBurstReceiver`: a frame with no CRC is never valid.** The gallery
    called `crc=none` "simply a shorter frame". The receiver still returns
    those bits unchanged, but its verdict is the frame description's
    (`dp_wfm_frame_desc_crc_ok`), so a description with no CRC stage is never
    `frame_valid` and every window is released (D3). The header, the property
    and the page now say so, and a C test pins it against a decoy
    ([#1769](https://github.com/doppler-dsp/doppler/issues/1769)).
