- **A Python receiver can ACQUIRE a frame, not just check one it was handed.**
    `doppler.detection.SyncFinder` correlates a known marker against every bit
    offset of a stream, in both polarities, and reports the first offset
    within a tolerance. `doppler.wfm.ccsds_asm_bits()` hands it CCSDS's
    `0x1ACFFC1D`. Together they close the gap
    [gh-900](https://github.com/doppler-dsp/doppler/issues/900) named: the
    frame checker `Frame.check()` had always existed, and nothing could find
    a frame for it to score, so everything the `ccsds_tm` certification
    measured about that detector described a function Python could not call.

    **The search is general and CCSDS is a configuration of it.** The kernel
    moved to a header-only `native/inc/dp_syncword.h`, and
    `ccsds_tm_asm_find` is now two lines over it — the same relationship
    `CCSDS_TM_CONV` has to `conv_code_t`. A standard picks a pattern; the
    correlation is not the standard's. `SyncFinder` therefore takes any
    marker, at any length, and `detection` stays free of any one document's
    picks.

    **`max_errors` is answered, not warned about.**
    [gh-897](https://github.com/doppler-dsp/doppler/issues/897) found that the
    threshold has to be chosen against the SEARCH WINDOW and that nothing said
    so: half of 32 is 16, so 8 "sounds safe", and at `t = 8` the marker is
    found at its true offset only 58 % of the time on a stream with no channel
    errors at all — each preceding offset is an independent chance to
    false-hit first. `SyncFinder.pfa(t)` is the per-offset false-alarm
    probability and `SyncFinder.max_errors_for(window_bits, pfa)` inverts it
    through `1 - (1 - pfa)**W`, returning the largest tolerance that still
    holds. They sit beside the search the way `det_threshold` sits beside
    `det_pd`, and they answer *for the marker being searched*, so a threshold
    and the thing it thresholds cannot come from two declarations.

    Checked against three independent oracles rather than against itself: an
    exhaustive enumeration of all 2^8 windows in C, `math.comb` in Python, and
    — the one that matters — **the search itself**, whose measured accept rate
    over 20000 random windows tracks the formula it is meant to size. Ten
    mutations of the kernel and three of the binding were each confirmed to
    turn the suite red.

    **A finding that was prose until now runs.** A complemented CADU passes
    its own outer code, cleanly, with nothing corrected: Reed-Solomon is
    linear and the all-ones vector is itself a full-length codeword, so a
    global flip lands on another codeword and the randomiser carries it
    straight through. A receiver that acquired at the right offset and ignored
    the reported polarity would score a clean PASS on a frame whose every
    payload bit is wrong. The headers have said the marker is "the only thing
    in a CADU that can say so" since the component was written; nothing had
    ever run it.

    The marker also stopped being transcribed. `0x1ACFFC1D` was expanded
    MSB-first by hand in six live places — two `frame_core.h` doctests that
    generated four more in `wfm.pyi`, two tests, a docs page and the link
    demo — which is exactly the hazard `ccsds_tm_asm_bits` exists to remove.
    All six now call `ccsds_asm_bits()`.
