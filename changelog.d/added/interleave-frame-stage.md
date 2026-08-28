- **`WFM_STAGE_INTERLEAVE` and `wfmgen --interleave R [--interleave-unit N]`.**
    A block interleaver over the frame's data group, applied after the outer
    code and before the inner one, so a burst on the channel arrives spread
    across codewords. Measured: with 5 × RS(255,223) it takes the corrigible
    burst from 16 octets to 80.
    [#1031](https://github.com/doppler-dsp/doppler/issues/1031)
