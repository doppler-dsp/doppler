- **`conv` — convolutional codes, both directions, on one code description.**
    A rate-1/n code is four numbers: a constraint length, an output count, a
    generator polynomial per output, and which outputs are inverted.
    `conv_code_t` holds them, `conv_outputs()` is the only place in the tree
    that says what the family emits, and **both** `conv_encode` and the new
    `viterbi_decode` read it. An encoder that computed the outputs and a
    decoder that computed them again would be two implementations of one
    primitive — and the inversion is exactly the detail that drifts between
    them, invisibly, because a matched pair decodes itself perfectly and
    interoperates with nothing.

    **CCSDS is now a configuration, not an implementation.** `FEC_CCSDS_CONV`
    is `{k=7, n=2, {0171, 0133}, invert G2}` and `fec`'s bespoke encoder is
    gone; the frame assembler encodes through `conv_encode`. Point the same
    objects at the deep-space rate-1/6 code, a K = 9 experiment, or anything a
    caller brings.

    The decoder is streaming and maximum-likelihood: 2^(k-1) states, branch
    metrics computed once per step from the `2^n` distinct output words rather
    than per state, path metrics renormalised so a stream cannot overflow them,
    and a traceback ring. It consumes the LLR convention `mpsk_soft_demap`
    produces, so it agrees with `mpsk_demap` on hard decisions by construction
    — and since scaling every branch metric cannot move the winning path, a
    caller with no SNR estimate may pass unscaled values. **Depth 60** is the
    measured default for CCSDS's code: `5·K = 35`, the textbook number, sits
    33 % above the achievable BER (`docs/design/viterbi.md` §4).

    **The external truth is the impulse response**, which is what a generator
    polynomial means — drive a 1 followed by zeros and output `j` traces
    `poly[j]`, inverted where the code says so. That is checkable for every
    configuration rather than only the familiar one, and it is not a round
    trip. Seven codes from K = 3 to K = 9 and rate 1/1 to 1/3 decode exactly;
    sabotage-proven four ways, each in the section that should catch it:
    reversing the state convention and dropping the inversion both redden the
    impulse response, while breaking the butterfly's input bit or shortening
    the traceback by one redden the decode.

    Also fixed on arrival, the gh-747 class: `conv_core` reached no library, so
    a C consumer could include the installed header and link none of its 11
    out-of-line symbols. jm 0.62.0's wiring check is what named it.
