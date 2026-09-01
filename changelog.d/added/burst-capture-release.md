- **`BurstCapture.release(i)` — a window that was not a burst gives its
    span back.** Detections inside an emitted window's span are HELD rather
    than dropped; a consumer whose error detection failed the frame releases
    it and the held detections are searched again. `DsssBurstReceiver` does
    this itself and reads back `frame_valid` (scalar and per event row), so
    a decoy ahead of a real burst no longer swallows it — the promise of
    #1004, restored under the capture.
    [#1181](https://github.com/doppler-dsp/doppler/issues/1181).
