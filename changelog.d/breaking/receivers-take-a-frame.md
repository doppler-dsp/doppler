- **`BurstDemod` and `DsssBurstReceiver` take the frame description.** The
    Python constructors take `frame` (a `Frame` or a `FrameDesc`) in place of
    a sync word and a hand-counted `frame_syms`: `BurstDemod(data_code,   frame, ...)` and `DsssBurstReceiver(acq_code, data_code, frame, ...)`.
    The sync word is the description's first field, the length is its layout,
    and a `DsssBurstReceiver` reads its CRC verdict from it (a description
    with no CRC stage is never `frame_valid`). `BurstDemod.set_sync()` is gone
    (one statement of the sync word, not two that could disagree), and a
    description whose first field is not known bits, is covered by a stage, is
    named `preamble`, or whose stages emit a new stream is refused, naming the
    rule ([#1620](https://github.com/doppler-dsp/doppler/issues/1620)).
