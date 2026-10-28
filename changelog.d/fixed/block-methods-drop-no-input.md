- **The halfband decimators, `DopplerChannel` and `MpskReceiver.bits` no
    longer drop input when `out` fills.** `hbdecim`, `hbdecim_r2c` and
    `hbdecim_q15` stop at a pair boundary and say what they took
    (`*_consumed()`), so a split call matches one whole call. Every resamp
    block form, `Resampler.consumed` and `DopplerChannel` report it too, and
    a call that takes nothing no longer resets `offset_hz`. `bits_max_out`
    takes the block length and returns `len * log2(M)` (#2643).
