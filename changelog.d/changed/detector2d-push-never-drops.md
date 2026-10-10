- **`dp_detector2d_push()` stops at a full result buffer where the input can
    resume, as `dp_detector_push()` does.** `dp_detector2d_consumed()` says
    where. In Python, `CorrDetector2D.push()` has room for 1024 detections
    instead of 64, and every later frame of a push that fills it is lost
    (#1992, just-buildit/just-makeit#2184). The state blob is version 2
    (#1895).
