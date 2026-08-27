- **`DsssBurstReceiver` and `BurstDemod` take a frame DESCRIPTION**
    ([#1017](https://github.com/doppler-dsp/doppler/issues/1017)). They
    assumed `sync | payload | CRC-16`, so a burst generated without a CRC
    decoded bit-exactly and was reported *invalid*, and one carrying an outer
    code could not be described at all. Both ends now build the same
    `wfm_frame_desc_t` from the same four choices — `crc`, `rs_depth`,
    `randomise`, `attach_asm` — so an RS-coded burst repairs before its
    payload is read and a randomised one derandomises. `set_sync()` is now
    `set_frame()`.
