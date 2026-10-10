- **`dp_detector_push()` no longer drops input when its result buffer
    fills.** It stops before the sample that would complete a frame it has no
    room for, and the new `dp_detector_consumed()` says where to resume; it
    used to discard the rest of the call. Python's `push()` still does
    (#1992). The detector's state blob is version 2 (#1895).
