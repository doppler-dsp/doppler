- **The frame assembler, and the ASM** (131.0-B-3 section 9) — the element the
    other four exist for. `fec_frame_encode` takes a Transfer Frame as packed
    octets and returns unpacked channel symbols, which makes it the one place
    the packed/unpacked boundary between `fec_rs.h` and `fec_ccsds.h` is
    crossed rather than a convention each kernel assumes about its caller.

    What it adds over running the four kernels in order is that **the stages do
    not all cover the same bits.** The marker enters third and the stages
    disagree about whether they reach over it: the outer code must not
    (9.5.1), the randomiser must not (10.3.4 note 1), and the inner code must
    (3.2.1, 9.2.1.4) — 9.2.1.5 states two of the three in a single sentence.
    So `fec_frame_layout` reports a **span per stage** rather than a stage
    order: an order is the representation that cannot express this, being
    right at three boundaries and wrong at the fourth, in the direction that
    still encodes, still decodes against a receiver of one's own construction,
    and syncs to nothing.

    All three rows are asserted against something outside the assembler: the
    marker byte for byte as figure 9-1 prints it, the randomiser's published
    40-bit prefix positioned *after* it — which fails in both halves if the
    randomiser reached back, since the marker would come out XORed with
    `FF 48 0E C0` and the block would begin at sequence bit 32 — and
    `fec_rs_codeword_ok`, which needs no decoder and refuses the 255 symbols
    taken from the marker rather than from behind it. Nine sabotages were run
    against the finished test, one per claim.

    Virtual fill (4.4.2's shortened codeblock) is not implemented, so a frame
    that is not exactly `223 * I` octets is refused rather than padded (#813).

    `fec_ccsds_core` now reaches `libdoppler.so` and `libdoppler.a`. It never
    had, for as long as `fec` has existed: `native/inc/fec/*.h` are installed
    headers whose every function is out-of-line, and `nm` found 13 defined
    symbols in the core against zero occurrences of `fec_` in the library — so
    a C consumer could include the header and link none of it. Python was
    unaffected the whole time, because the extension links each core directly,
    which is exactly why nobody noticed. jm 0.62.0's wiring check is what
    named it.
