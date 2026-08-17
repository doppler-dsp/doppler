- **The Viterbi decoder resumes from a blob, like every other stateful
    object.** `viterbi_state_bytes` / `viterbi_get_state` / `viterbi_set_state`
    carry the path metrics, the traceback ring and the cursor into it, so a
    decode split anywhere and restored into a *fresh* decoder produces the same
    bits as one uninterrupted pass. A decoder is a link in a chain — behind the
    receiver, in front of the R-S decoder — and a chain is checkpointable only
    if every link is; one that is not is enough to make elastic resume
    unavailable for everything it sits between.

    **`fill` travels, because it is part of the answer rather than
    bookkeeping.** A decoder resumed inside its first `depth` bits still owes
    its traceback and must emit nothing for bits it has not earned;
    `viterbi_decode_max_out` reads `fill` to say so. A blob that dropped it
    resumes a decoder that invents them, and a split taken in steady state
    cannot see the difference — so the test takes two, one on each side of
    `depth`.

    **A size match is not a configuration match.** The code and the depth are
    configuration, restored by `viterbi_create` rather than carried in the
    payload — but they are *stamped* in it and compared, because two codes with
    the same `k` and `n` differing only in a polynomial or in `invert` produce
    blobs of identical length. The envelope's own size check sees no
    difference between them, and reinterpreting one as the other yields a
    decoder that is confidently wrong instead of one that refuses.

    Eight guards, each proven by sabotage and each reddening in the section
    meant to catch it: dropping `fill` shortens the split stream, dropping the
    cursor or either buffer breaks bit-exactness, and removing the code
    comparison lets an uninverted-CCSDS blob restore into a CCSDS decoder.
    Closes #824.
