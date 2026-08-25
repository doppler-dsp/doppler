- **`BurstDespreader` is certified, and its lock gate is now measured rather
    than asserted** —
    `src/doppler/dsss/tests/validation/burst_despreader/results.md`, 15
    limits on every push, plus three C sections.

    **The gate delivers the false-alarm rate it prices.** The header states
    the null law exactly — `R² = stat_n · F(stat_n, stat_n)`, *not*
    chi-square, because the noise reference is estimated from as many
    samples as the signal sum — and names the gate as
    `R > sqrt(stat_n · det_threshold_f(pfa, stat_n))`. Nothing had checked
    that the two agree. Over **4000 noise-only bursts** the realized rate is
    within **7%** of the priced rate across a decade of `pfa` (1.04 / 1.01 /
    0.98 / 1.07). That closes `detection`'s `det_threshold_f` work against a
    real consumer instead of synthetic draws of an assumed distribution.

    **The header's own example asserted a false result, and no gate ran
    it.** `lock_stat`'s `@code` block fed a perfectly noiseless burst and
    asserted it passes the pfa=1e-3 gate; it returns `False`. A noiseless
    input makes the quadrature sum exactly zero, and `lock_stat` then
    returns its 0 sentinel — so the **worst** reading of the statistic is
    what a **perfect** synthetic burst produces. The example escaped
    `make test-stubs` because a `@code` block on a `*_get_*` accessor is not
    transplanted into the property's docstring:
    [**46 header examples across the library are in that blind spot**](https://github.com/doppler-dsp/doppler/issues/1000).
    All 46 were extracted and executed by hand during this certification —
    the other 45 pass. The example now carries a noise floor, which is the
    only condition under which a lock statistic means anything, and the
    overloaded sentinel is documented and pinned in C.

    **`set_acq` was called twice in the C test with no assertions**, so the
    claim it exists to support — that only payload prompts fold into the
    statistics, *"so the H0 law and the SNR calibration hold"* — was pinned
    by nothing. A preamble prompt integrates a different code length and
    sits inside the pull-in transient, so including it breaks the very law
    measured above. Now checked as an inequality against the same stream fed
    without the preamble declared, and sabotage-proven by dropping the
    exclusion.

    **Only the locked end of `lock_metric` was pinned**, at `> 0.9`. The
    header documents both ends, and the unlocked one is 2/π = 0.6366 — not
    far below 0.9, so a metric that had stopped responding to the carrier
    had room to sit unnoticed. Both are now measured (0.636 with no carrier,
    1.000 clean) and sabotage-proven by pinning the metric to 1.0. Pinning
    the *unlocked* value is what makes the locked one evidence: the two must
    be separated, not merely both plausible.

    Also certified: `snr_est` rises monotonically as `bn_carrier` narrows —
    it is the *effective* post-loop SNR, below the AWGN value by the jitter
    term, which is the number that predicts demodulation rather than the one
    that flatters the link; and `reset()` re-arms the cumulative statistics,
    which is required between bursts because `set_acq` re-arms the preamble
    only.
