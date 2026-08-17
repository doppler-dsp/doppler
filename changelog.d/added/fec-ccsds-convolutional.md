- **The CCSDS rate-1/2 K=7 convolutional code** (131.0-B-3 section 3.3), with
    the symbol inversion on the G2 output path that 3.3.1(5) requires. That
    inversion is invisible to a round trip — a matched Viterbi inverts whatever
    it was handed — so it is pinned against the impulse response, where C1 must
    trace `G1 = 1111001` and C2 the complement of `G2 = 1011011`, and against
    an all-zero input, which must emit C1 all zeros and C2 all ones.
