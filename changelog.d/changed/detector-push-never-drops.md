- **A full result buffer stops `dp_detector_push()` where the input can
    resume.** It used to drop the rest of the call; `dp_detector_consumed()`
    now says where to resume. In Python, `push()` has room for 1024
    detections instead of 64. A push that would make more now LOSES the
    frames past it, where it used to keep them for the next call (#1992).
    The detector's state blob is version 2 (#1895).
