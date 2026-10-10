- **A full result buffer stops `dp_detector_push()` where the input can
    resume** (`dp_detector_consumed()`), keeping a rest shorter than a frame
    as the carry. Python's `push()` has room for 1024 detections (was 64): a
    push that completes at most 1024 frames, counting the carry, loses
    nothing. Past that its later frames are lost, and the next push stays
    on the frame grid only when what was lost is a whole number of frames
    (#1992, just-buildit/just-makeit#2184). State blob v2, without
    `last_corr` (#1895).
