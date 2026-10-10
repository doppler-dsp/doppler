- **A frame whose sample count wraps is refused at both ends.** A receiver
    checked `num_samples * elem_size` against the payload in 64 bits, so a
    forged count that wrapped onto the payload size was believed and
    `dp_msg_num_samples()` reported a count no buffer held; it now checks
    by division. A sender whose count wrapped under the 32-bit payload
    limit sent such a frame; it now returns `DP_ERR_TOO_LARGE` (#2016).
