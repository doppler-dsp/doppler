- **`docs/design/viterbi.md` and `docs/design/fec-receive.md` — the design
    for the FEC receive half.** The decoder gets its own page; the chain page
    owns the synchronization and the lock detector.
    Planned, not built. `fec/` encodes today and decodes nothing, so nothing
    can measure a coded link and coding gain is unquotable.

    The prototype decoded symbols dumped from the **shipped** `fec_conv_encode`
    rather than from a re-derivation of it, and **refuted two things a first
    sketch had assumed**:

    - **The code is transparent, so the decoder cannot resolve polarity.** Both
        generators have odd weight (5), so inverting the input inverts both
        outputs and an inverted stream is an exact codeword of the inverted
        bits — measured: `decode(-llr)` returns exactly `~bits`. The sketch had
        node sync searching phase × polarity and picking the best; measured,
        the two polarities read 0.1193 and 0.1197, i.e. it would have been
        choosing from noise. **Polarity belongs to the ASM search**, which must
        correlate for the marker and its complement. Phase does separate, ~2–4×.
    - **The node-sync statistic's form is open, and deliberately unevaluated.**
        Mengali, Pellizzoni & Spalvieri (IEEE T-COMM 43(9), 1995) is the
        authority on the soft-decision form; this design does not implement it.
        Three ad-hoc comparators — the hard disagreement count and two soft
        ones — measured **within noise of each other** (17/18/18 wrong
        decisions out of 300 at 32 bits, 6/7/7 at 64), so the hard count is
        chosen for being the cheapest of three that tie, not for winning.

    Also measured: **`5·K` traceback is 33 % above the BER floor** (0.04178 vs
    0.03137 at 1 dB); depth 60 is within 3 %. And the detector is sized for
    **both** error probabilities, including the one nobody sizes —
    **P_false_unlock**, dropping lock on a working link. At a 0.30 threshold a
    Gaussian puts it at 5.17e-5 where the measurement says 2.50e-3: **48×
    optimistic**, in the direction that promises a link that does not drop.

    **The lock operating point is recorded with what it buys**: window = 500
    channel symbols, threshold = 100 (20 %). The in-sync statistic tracks the
    theoretical channel symbol error rate to within a count (66.6 against 65.5
    predicted at 1 dB; 18.8 against 18.8 at 5 dB), so "sync metric, lock
    statistic and channel quality" really are one quantity. Against false
    *unlock* the threshold is excellent — 1.5 % at 1 dB, below 1e-3 from 2 dB
    up. Against false *lock* a single window is marginal at 12.7–18.8 %,
    because the out-of-sync distribution is ~125 ± 25 and the threshold sits
    about 1σ below its mean; that is the argument for `lockdet`'s hysteresis
    rather than against the threshold.

    The measurement home is settled too, and it is **not a new sweep**: the
    receiver instrument built on `docs/design/rx-test.md` already owns the
    stimulus, the statistics and the frame outcomes, so a coded link is an
    adapter and an operating point — the way `ContinuousMpskReceiver` was.
