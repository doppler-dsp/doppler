- **`PolynomialPhaseEstimator` is certified** —
    `src/doppler/dsss/tests/validation/ppe/results.md`, 15 limits on every
    push, plus five C sections its 74-line test was missing. Two of its
    header claims were untestable at the tolerances in place, and one was
    simply wrong.

    **The sub-bin refinement was pinned at a tolerance 2.6 BINS wide.** The
    header claims the peak is "refined sub-bin in both axes by parabolic
    interpolation"; the C test's `ftol` of 5e-3, against a bin of
    1/512 = 1.95e-3, is 2.6 bins — wide enough to pass with the refinement
    deleted and the raw argmax returned. Measured, the estimator resolves a
    noiseless tone to about **1e-4 of a bin**, and stays within 0.05 of a
    bin at 0 dB input SNR. The gate is now 0.01 bins: a hundred times
    tighter than the old tolerance and a hundred times looser than the
    object achieves. Sabotage-proven by forcing the raw-argmax fallback,
    which takes the new section red and leaves every pre-existing assertion
    green.

    **`nfft` was documented wrong by a factor of four.** The struct comment
    read "next pow2 of max_len"; the implementation uses
    `next_pow2 (max_len) << 2`. Not cosmetic — `nfft` sizes `buf`, `spec`
    and `mag`, so a caller budgeting memory from the header was out by 4x on
    three buffers, and the same 4x is what makes the sub-bin accuracy above
    what it is. The comment was hiding the mechanism as well as the
    footprint. Corrected and pinned so the two cannot drift apart again.

    **`snr_db` was measured by nothing, and it is not an input SNR.** It is
    a peak-to-mean taken *after* the coherent transform, so it carries the
    processing gain: quadrupling the segment adds ~3.7 dB on identical
    input. A caller thresholding on it as though it were the segment's SNR
    is comparing an integrated quantity against an input-referred one, and
    that threshold then moves whenever the segment length does. Now
    asserted as the scaling relationship rather than a literal, which would
    pin the noise draw as much as the estimator.

    Also now pinned: `freq_norm` stays in `[-0.5, 0.5)` **with the sign
    intact** at both shoulders (an off-by-one in the bin-to-frequency map
    wraps a near-Nyquist tone to its image, which reads downstream as a
    receiver locking to the negative image); the documented input floor of 4
    samples zeroes every field below it *and* estimates at the boundary, so
    the refusal is a boundary rather than a blanket; `reset()` really is the
    documented no-op; the estimate is invariant to a constant phase and to
    amplitude scaling over four decades, `snr_db` included; `n_rate` is odd
    at every `max_rate`, which puts `r = 0` on a grid node; and the header's
    caller-facing contract that squaring a BPSK stream returns **2f and 2r**
    is measured — halved, the frequency lands within 0.04 of a bin.

    One thing recorded rather than changed: the frequency axis's sub-bin
    refinement is **not in this object**. `ppe_estimate` delegates to
    `find_peaks_f32` from `spectral_core` and falls back to a raw argmax
    only if that returns nothing. The header is accurate about the behaviour
    and quiet about the ownership; re-inlining it would be exactly the
    duplication the library's own rule forbids. The consequence worth
    knowing is that a regression in `find_peaks_f32` surfaces as a `ppe`
    accuracy failure.
