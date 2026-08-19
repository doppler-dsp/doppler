- **`conv` is certified — the first component with no Python face to be.**
    The campaign's evidence layer is a Python validator, and `conv` has no
    binding: a binding built only to be measured is one nobody calls, and the
    campaign would then be certifying an artifact of its own process. So the
    substitution is **C measures, Python renders**.
    `native/validation/conv_certify.c` runs the sweeps — bits from `pn`,
    symbols from `mpsk_map`, noise from `awgn`, soft decisions from
    `mpsk_soft_demap` — and emits CSV;
    `src/doppler/tests/validation/conv/validate.py` parses it and
    characterises, reviews and asserts through the same `Report` every other
    object uses, so the format cannot drift between the two kinds. Nothing in
    the C decides whether a number is acceptable; nothing in the Python
    computes one. `docs/dev/contributing/validation.md` and
    `docs/dev/contributing/adding-algorithms.md` carry the track, both gates found it by
    glob with no registration, and the validation log reads **12 objects
    certified**.

    **What the report says a caller should do.** Ship traceback depth 60
    rather than the textbook `5*K = 35`, which sits 17 % above the achievable
    floor where 60 is within 1.2 %. Feed the decoder soft decisions or do not
    code at all — below Eb/N0 ~3.5 dB a hard-decision Viterbi is **worse than
    an uncoded link**, because the rate costs 3.01 dB of Eb that a two-level
    input does not buy back. Size a node-sync window for the job it has: the
    phase decision holds at 250 bits, while the in-sync statistic only becomes
    a channel estimate — to within 25 % of the delivered symbol error rate —
    at a thousand bits or more.

    **And one design number is corrected.** `docs/design/viterbi.md` §4
    quoted `5*K` at 33 % above the floor (0.04178 against 0.03137), from a
    prototype that was explicitly throwaway and uncommitted. Measured over
    four times the bits at the same Eb/N0: 17 %, with every level ~30 %
    higher — about what a fraction of a dB of Es/N0 convention is worth on a
    curve that steep. The page now carries the table the gated harness
    produces and says the prototype's is superseded; the decision it drove is
    unchanged.
