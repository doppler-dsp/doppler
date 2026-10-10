- **A full result buffer stops `dp_detector2d_push()` where the input can
    resume** (`dp_detector2d_consumed()`), keeping a rest shorter than a
    frame as the carry. `CorrDetector2D.push()` has room for 1024 detections
    (was 64): a push that completes at most 1024 frames, counting the carry,
    loses nothing, and past that its later frames are lost (#1992,
    just-buildit/just-makeit#2184). State blob v2, without `last_corr`
    (#1895).
