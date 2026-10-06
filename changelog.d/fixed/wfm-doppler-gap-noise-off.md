- **wfm: a Doppler burst with `gap_noise="off"` and a delay is no longer lost**
    (follow-up to doppler#1858). The fix for the gap leak declared the delay to
    the channel even when the gaps are never pulled, which fed the burst
    `delay` samples late. With the gaps off the channel's input starts at ON.
