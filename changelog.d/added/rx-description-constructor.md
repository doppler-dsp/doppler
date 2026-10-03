- **Receivers can be built from the frame description the transmitter spread.**
    `dp_burst_demod_create_desc` and `dp_dsss_burst_receiver_create_desc` (C)
    take the `wfm_frame_desc_t` and derive the sync word (field 0) and the
    frame length from it, through the new `dp_wfm_frame_desc_rx`, which
    refuses a description whose field 0 is not known bits, is covered by a
    stage, is named `preamble`, or whose stages emit a new stream. A
    `DsssBurstReceiver` built this way reads its CRC verdict from the
    description. Byte-identical to the sync-word constructors on every
    existing fixture; the old constructors stay for now
    ([#1620](https://github.com/doppler-dsp/doppler/issues/1620), step 2).
