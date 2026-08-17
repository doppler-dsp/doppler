- **The CCSDS receive chain, and a link demo that runs it end to end.**
    `ccsds_tm_frame_decode` is the mirror of `ccsds_tm_frame_encode` — it reads the same
    `ccsds_tm_frame_cfg_t` and the same span table, so the two directions cannot
    come to disagree about which stage covered what, which is the failure
    `ccsds_tm_frame.h` opens by describing. It skips the marker, re-applies the
    involutive randomiser over the block span, packs back to octets MSB-first,
    and checks each interleaved codeword; `ccsds_tm_frame_rx_t` reports what it
    found. `ccsds_tm_asm_find` correlates for the marker at every offset in
    **both polarities**, because a BPSK carrier recovered through a 180-degree
    ambiguity delivers the stream complemented and the marker is the only part
    of a CADU that can say so — the randomiser deliberately does not cover it.

    **The outer code is a check, not a correction.** `ccsds_tm_rs_codeword_ok` is a
    syndrome test; Berlekamp-Massey, Chien and Forney are still
    `docs/design/fec-receive.md` §7 step 2. So `rs_ok < rs_codewords` means the
    returned frame is wrong in a way the function knows about, which is why it
    is reported rather than folded into the return value.

    **The inner decode is deliberately outside it.** A Viterbi is streaming and
    emits decision `i` only after `depth` further bits, and the marker that
    says where a CADU *starts* is only readable once the inner code is undone —
    so a function taking channel symbols would have to own a decoder, a search
    window and a buffer. That is a streaming receiver object; this is the pure
    per-frame chain it will call. It matches the encoder, where `conv_enc_t`
    belongs to the caller for the same reason: the inner code is continuous and
    the frame is not.

    `examples/c/ccsds_link_demo.c` runs the whole thing — R-S, randomiser, ASM,
    K=7 rate-1/2, BPSK, AWGN, soft demap, Viterbi, sync, and back — and prints
    an Es/N0 sweep plus the recovered text. It measures rather than asserts:
    the channel's own symbol error rate comes out at **7.90 % at 0 dB against
    Q(sqrt 2) = 7.86 %**, the inner code takes that to **zero bit errors in
    40092 from 2 dB up**, and the outer code reports 7 of 10 codewords surviving
    at 0 dB where the Viterbi did not clear the channel. The capture starts
    1554 symbols late on purpose, so the sync has to find the frame rather than
    be told: it reports the marker at bit 9455, which is exactly
    `10232 - 777`.

    Nine guards, each proven by sabotage. Two of them were GREEN on the first
    attempt and the tests were what changed: a rotated de-interleave is
    **invisible against a zeros payload**, because R-S of all-zeros has
    all-zero parity and every interleaved column is then identical — the zeros
    this file uses to keep a missing randomiser visible were hiding a different
    defect, so that section now pays for it with structured data. And the ASM
    search was never exercised at the last offset a marker can occupy, so a
    loop bound of `<` instead of `<=` passed everything while losing exactly
    the frame flush against the end of a capture.
