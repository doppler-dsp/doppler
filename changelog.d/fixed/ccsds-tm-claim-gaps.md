- **Four `ccsds_tm` header claims that nothing asserted.** The claim
    inventory `docs/dev/validation.md` step 1 asks for, run against
    `ccsds_tm`'s three headers, found four rows the C tests did not cover —
    two of them the shapes that page warns about by name.

    **The dual basis was a consistency test.** Requiring the two transforms
    to invert each other is satisfied by *any* invertible 8×8 GF(2) matrix
    and its inverse, so it could not see a defect the two halves share.
    Demonstrated rather than argued: reading 4.3.9.3's two equations the
    wrong way round — the likeliest transcription error — leaves an exact
    inverse pair and the old check stayed green.

    Replaced by a **derived** check. Every GF(2)-linear functional on GF(2⁸)
    is `u -> Tr(c·u)` for a unique `c`, so the transform's eight output bits
    are eight field elements; the test solves for them from the shipped
    matrix and the shipped field — using `rs_core`'s own tables, not a
    private multiply — and asserts the structure a dual basis has: `c_0 = 1`,
    `c_j = c_1^j`, and `Tr(c_i · β_j) = δ_ij` read through the *other* matrix
    so both transcriptions are covered. Measured, `c_1 = α^117`, which is
    **not** primitive (`gcd(117, 255) = 3`) and does not need to be.

    What that still cannot catch — the standard specifying a different
    generator — is [gh-861](https://github.com/doppler-dsp/doppler/issues/861),
    and the header no longer claims otherwise.

    **`asm_find` promises FIRST below threshold, not best**, and nothing
    tested it: every case put one marker in a zero background, where the two
    are the same offset. Now two markers with the *earlier* one damaged, plus
    the same stream at a tighter tolerance so a search hard-wired to the
    first offset fails too. It matters because a best-match search has to see
    the whole stream before it can answer, which a frame synchroniser on a
    live capture cannot do.

    **The interleaver's differential ran no library code.** The section named
    "what interleaving is FOR" computed `b % DEPTH` in a loop and asserted
    arithmetic about its own loop — it held for any interleaver, including
    one that did not interleave. Now measured through `encode_block` /
    `decode_block`: a burst of `depth × E` is repaired in full and one symbol
    more costs exactly one codeword, at **every** depth 4.3.5.1 allows —
    which also closes depths 2, 3, 4 and 8, exercised nowhere before — plus
    the differential itself, an 80-symbol burst that depth 5 carries and
    depth 1 refuses at identical rate.

    Every one proven by sabotage: a flipped matrix bit, a self-consistent
    wrong pair, a best-match search, an interleaver that does not interleave,
    and an accepted out-of-range depth.
