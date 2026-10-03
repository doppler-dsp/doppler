- **`BurstDemod` and `DsssBurstReceiver` take the frame description.** The
    Python constructors take `frame` (a `Frame` or a `FrameDesc`) in place of
    a sync word and a hand-counted `frame_syms`: `BurstDemod(data_code, frame, ...)` and
    `DsssBurstReceiver(acq_code, data_code, frame, ...)`.
    The sync word is the description's first field, the length is its
    layout, and a `DsssBurstReceiver` reads its CRC verdict from it (no CRC
    stage, never `frame_valid`). `BurstDemod.set_sync()` is gone, and a
    description the receiver cannot use is refused, naming the rule ([#1620](https://github.com/doppler-dsp/doppler/issues/1620)).
