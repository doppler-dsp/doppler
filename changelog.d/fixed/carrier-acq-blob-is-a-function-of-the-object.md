- **`CarrierAcquisition`'s state blob is a function of the object.**
    `get_state` wrote the whole carry buffer, though only its first
    `carry_len` samples are state, so two identical objects gave different
    blobs from whatever the buffer held before. It now writes zeros past
    the carry; the blob's size and `set_state` are unchanged (#2076).
