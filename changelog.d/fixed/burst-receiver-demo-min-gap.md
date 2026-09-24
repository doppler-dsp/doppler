- **The DSSS burst receiver demo spaces its bursts by `min_gap`**
    ([#1514](https://github.com/doppler-dsp/doppler/issues/1514)). It used
    `refine_span`, a start-to-start reach, and called it the minimum spacing.
    It now asserts the `min_gap` guarantee and counts only `frame_valid`
    frames. It no longer claims that bursts packed tighter are lost; that
    loss was a defect ([#1527](https://github.com/doppler-dsp/doppler/issues/1527)).
