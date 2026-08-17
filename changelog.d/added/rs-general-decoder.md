- **Reed-Solomon is a description now, and it CORRECTS.** `rs/rs_core.h` is a
    general Reed-Solomon code over `GF(2^J)` — a symbol width, a field
    polynomial, a parity count, a first root and a root stride — with the
    encoder, the syndromes and a Berlekamp-Massey / Chien / Forney decoder all
    reading the same `rs_code_t`. **Nothing in it is CCSDS.** 131.0-B-3's
    (255,223) `E = 16` is `CCSDS_TM_RS`, five numbers in `ccsds_tm/ccsds_tm_rs.h`,
    beside the two things the standard adds that are not properties of the
    code: the dual-basis symbol representation (4.3.9) and the interleaver
    (4.4.1). Its own `c_dep`, for the reason `conv` is one — a caller who
    wants a Reed-Solomon code should not link a channel-coding standard.

    This closes [#826](https://github.com/doppler-dsp/doppler/issues/826): the
    outer code checked and could not correct, so all the concatenated coding
    gain past the Viterbi was unavailable. In `examples/c/ccsds_link_demo.c`
    at `Es/N0 = 0 dB`, where the inner code does not clear the channel, the
    three symbol errors it lets through used to cost **three of ten
    codewords** and a frame that was wrong-but-known-wrong; they are now
    repaired and all ten codewords are good, frames byte-exact.
    `ccsds_tm_frame_rx_t` grew `rs_corrected` and `rs_symbols` to report the repair
    work, and `rs_ok` now counts codewords valid **after** decoding.

    **Two offsets a textbook will not warn about**, both of which produce a
    decoder that decodes its own encoder perfectly and interoperates with
    nothing — the failure this slice keeps finding in new guises. First: the
    syndromes are a power sum only after substituting `Yt = Y * X^j0`, so
    running Berlekamp-Massey while assuming `j0 = 1` finds the right error
    *positions* and magnitudes wrong by `X^(j0-1)`, and every syndrome still
    checks out against the decoder's own model. Second: Chien iterates the
    **position exponent** rather than field elements, which is what makes a
    root stride of 11 cost nothing — a search over `a^e` has to invert the
    stride to learn where the error is. Both are derived in
    `docs/design/reed-solomon.md`, which owns the outer code the way
    `docs/design/viterbi.md` owns the inner one.

    **Validated rather than trusted:** `rs_init` refuses a non-primitive field
    polynomial (the table must visit every nonzero element exactly once) and
    `rs_code_valid` refuses a root stride sharing a factor with `n`, because
    both produce arithmetic that is entirely self-consistent — CCSDS 4.3.4
    states the second as a note about `a^11`, and for a general implementation
    a note is a condition to check.

    The external truth is the code's own distance rather than a round trip:
    `E` symbol errors corrected exactly, the sent word **never** recovered at
    `E+1`, refusal as often as `~1/E!` says, and — provable, and proved —
    a decode either refuses or returns a codeword, never a third thing, with a
    refused word left untouched. Checked at three configurations (textbook
    RS(255,223), the CCSDS-shaped one, and RS(15,11) where every single-symbol
    error at every position and value is swept). Every guard proven by
    sabotage, including one that was NOT: a zero-magnitude refusal branch that
    survived 600k adversarial patterns without ever executing, deleted rather
    than left as a claim nothing runs.
