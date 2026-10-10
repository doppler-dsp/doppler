- **A `DopplerChannel` that refuses a state blob is left as it was.** It
    wrote its own counters and profile before its resampler could refuse
    the blob, so a refused restore left them holding the blob's values. The
    resampler now restores first, and the channel's fields are written only
    once it has accepted (#2104).
