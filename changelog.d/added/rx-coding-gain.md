- **Coding gain, measured through a real receiver.**
    `native/validation/rx_coding_gain.c` runs the whole CCSDS chain in both
    directions with a demodulator in the middle — R-S, randomiser, ASM, K=7
    r=1/2, BPSK, RRC, AWGN, `MpskReceiver`, soft demap, node sync, Viterbi,
    ASM search, derandomise, R-S **decode** — and reports what each stage
    saw. It is an adapter and an operating point rather than a second
    harness: the receiver adapters moved to `native/tests/dp_rx_mpsk.h` and
    are shared with `rx_battery.c`, and the point is `DP_RX_ANCHOR` with one
    field changed, so a difference from the battery's numbers is the coding
    or the Es/N0 and cannot be the geometry.

    At Es/N0 = +2 dB (Eb/N0 5.59 dB), with the channel putting **one symbol in
    25** wrong before decoding, the link delivered **46 of 46 frame slots,
    every one byte-exact, 0 payload errors in 410 320 bits** — a coding gain
    of **≥ 4.1 dB**, where the bound is the run length rather than the code:
    zero errors is not a rate, so it is the exact 95 % upper limit
    (`ber_confidence`) turned into the Eb/N0 an uncoded link would have
    needed (`ber_esn0_db_for_ser`), minus the Eb/N0 this link ran at. The
    rate is charged first — R = 1/2 × 223/255, so 3.59 dB of redundancy
    before any gain is claimed.

    **Three things only a receiver-in-the-loop run could say**, all now in
    `docs/design/fec-receive.md` §8. The uncoded lock detector is not a
    usable gate for a coded link: the binary `locked` flag reads 23 % at 0 dB
    while the loops track throughout and every delivered frame is byte-exact,
    so the window is the settling budget and the evidence of lock is that the
    marker appears. Slips are real at these Es/N0 — frame sync loses the
    marker where it expected it 4 times over ~46 slots at 0 dB, falling to
    zero only at the clean point — and a measured slip moved the stream by an
    ODD number of symbols, which flips the `(C1, C2)` parity and makes every
    subsequent bit noise, so node sync cannot be a one-shot at start of
    stream. And the outer code **never miscorrected**: a CADU that
    decodes, reports every codeword good and matches no transmitted frame is
    the failure `rs_core.h` warns is possible past `E`, and across the whole
    sweep including the points where nothing synchronised there were zero.

    Five gates, each proven by sabotage: disabling R-S correction, sweeping
    only an easy link (which fires the channel-SER, gain-bound and
    waterfall-span gates at once), and forcing the node-sync hypothesis test
    to a tie.
