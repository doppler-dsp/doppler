- **`conv`'s claim inventory, and the four things it found.** The first two
    steps of `docs/dev/contributing/validation.md` — enumerate the header's claims, map
    each onto `test_conv_core.c` as pinned / pinned-only-at-literals / absent,
    then write and **sabotage** a test for every uncovered row.

    **`conv_outputs` and `conv_next_state` had zero mentions.** The file
    docstring calls the first "the only place that says what this family of
    codes emits" and the register convention "load-bearing", and both were
    exercised only *through* `conv_encode` — the one caller that agrees with
    them by construction. A user building a trellis (which is what
    `viterbi_create` does) reads them directly. Now the trellis is run BY
    HAND from the two of them and required to reproduce `conv_encode` symbol
    for symbol, at three codes, plus that a state IS the `k-1` previous
    inputs with the newest in the high stage.

    **The LLR sign convention was pinned only against the test's own
    helper.** Every section fed LLRs through one `to_llr`, so a decoder and a
    helper that flipped TOGETHER passed all of them — measured, not
    theorised: flipping both leaves every pre-existing section green. The
    identity code (`k=2, n=1, poly={0b10}`) closes it without importing
    anything, because a maximum-likelihood decode of the identity code is
    exactly a hard slicer, so the decoded bits must equal `llr < 0` element
    for element.

    **`d_free` was an explicit unknown** (`docs/design/viterbi.md` §8) and is
    now measured against published values: **10** for CCSDS's (171,133) K=7,
    5 for the K=3 (7,5), 6 for the K=4 (15,17).

    **And one claim was simply wrong.** The header said "the first `depth`
    bits of a stream produce no output"; the traceback walks `depth - 1`, and
    `viterbi_decode_max_out` agreed with the code. The test pinned the two
    against each other, so nothing could see the prose was off by one — 493
    bits come out of 500 symbols at depth 8, not 492. The prose moved, and
    the count is now pinned against a literal as well as against the sizing
    function.
