- **`PSD` takes a frame hop and carries its partial frame.** `dp_psd_create_hop()`
    (and `PSD(hop=...)`) sets the spacing between frames, from 1 to the window
    length (0 means the window length, as before). `dp_psd_accumulate()` no
    longer drops a trailing partial frame: the framer keeps it for the next
    call, so any chunking of a stream averages to the same result as one call.
    A single call of whole frames at hop = n is unchanged, bit for bit (#1894).
