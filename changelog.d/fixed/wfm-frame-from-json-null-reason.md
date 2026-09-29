- **`dp_wfm_frame_from_json(NULL, &why)` names a reason.** It refused
    without setting `why`, which the header promises on every failure.
