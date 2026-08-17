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
    - **A naive soft sync statistic is worse than the plain hard count.**
        Mengali, Pellizzoni & Spalvieri (IEEE T-COMM 43(9), 1995) is the
        authority on the soft-decision form; the ad-hoc variant tried here
        separates about 2× wider and yet decides *worse* at short observations
        — 18 wrong decisions against 9 over 64 bits. Adopting "soft is better"
        on the strength of a title would have made the synchronizer worse, so
        the statistic's form is an open item with a named reference.

    Also measured: **`5·K` traceback is 33 % above the BER floor** (0.04178 vs
    0.03137 at 1 dB); depth 60 is within 3 %. And the detector is sized for
    **both** error probabilities, including the one nobody sizes —
    **P_false_unlock**, dropping lock on a working link. At a 0.30 threshold a
    Gaussian puts it at 5.17e-5 where the measurement says 2.50e-3: **48×
    optimistic**, in the direction that promises a link that does not drop.
