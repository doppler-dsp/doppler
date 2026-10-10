- **A full result buffer stops `dp_detector_push()` where the input can
    resume.** It used to drop the rest of the call; `dp_detector_consumed()`
    now says where to resume. In Python, `push()` has room for 1024
    detections instead of 64. Once a push fills it, every later frame of that
    call is lost, detection or not; v0.65 kept up to `ring_cap/n - 1` of them
    for the next call and dropped the rest (#1992,
    just-buildit/just-makeit#2184). The state blob is version 2 (#1895).
