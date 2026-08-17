- **`fec` — channel coding, starting with the CCSDS pseudo-randomiser.**
    doppler encoded nothing before this: no convolutional code, no
    Reed-Solomon, no interleaver and no randomiser anywhere in the tree, which
    is the gap between a test-vector generator and a link waveform. The first
    element of the CCSDS TM slice (131.0-B-3, section 10) lands as its own
    `c_dep` rather than a library inside `wfmcompose`, because both ends want
    it — wfmgen encodes and `frame`/`ber_meter` will decode, and a receiver
    should not carry the waveform-composition chain on its link line for the
    sake of a randomiser.

    `fec_ccsds_randomise` is its own inverse, so both ends call one function.
    The published 40-bit prefix is what pins it: a first cut transcribed the
    taps from the polynomial's exponents rather than from the recurrence they
    stand for, drove the register to the all-zero fixed point, and **passed
    both a round trip and both period checks** — a dead sequence repeats with
    period 255 and matches no earlier one. The period test now also asserts
    the 128-ones balance a maximal generator must have, which no degenerate
    sequence does.
