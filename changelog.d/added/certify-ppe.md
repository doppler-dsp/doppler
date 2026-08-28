- **`PolynomialPhaseEstimator` is certified** — 15 limits on every push. The
    sub-bin refinement had been pinned at a tolerance of **2.6 bins**, wide
    enough to pass with the refinement deleted and the raw argmax returned;
    measured, the estimator resolves a noiseless tone to ~1e-4 of a bin and
    stays within 0.05 of a bin at 0 dB input SNR. One header claim was simply
    wrong: `nfft` is 4x next-pow2, not next-pow2, so a caller budgeting memory
    from the header was out by 4x. Evidence:
    `src/doppler/dsss/tests/validation/ppe/results.md`.
