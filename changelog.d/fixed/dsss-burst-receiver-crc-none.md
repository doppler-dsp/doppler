- **`DsssBurstReceiver`: a frame with no CRC is documented as never
    valid.** The gallery called `crc=none` "simply a shorter frame". The
    receiver still returns those bits unchanged, but it reads the last 16
    symbols as a CRC-16, so `frame_valid` is 0 and every window is released
    (D3). The header, the property and the page now say so, and a C test
    pins it against a decoy
    ([#1769](https://github.com/doppler-dsp/doppler/issues/1769)).
