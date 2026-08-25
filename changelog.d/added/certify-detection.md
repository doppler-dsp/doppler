- **`detection` is certified** — the sizing helpers every detector in the
    library shares now carry evidence:
    `src/doppler/detection/tests/validation/detection/results.md`, 20 limits
    asserted on every push, plus a design page and a Monte-Carlo
    characterization subject that neither existed before.

    **Why this object first.** `Acquisition`, `LockDet`, `Dll`,
    `BurstDespreader` and `BerMeter` all size themselves here, so a defect in
    these 19 functions moves every detector at once — and the object had no
    design doc, no certification, and three load-bearing claims pinned in no
    C test at all.

    **What the claim inventory found.** Four things, all fixed:

    - **The non-coherent trio was pinned in Python only.**
        `det_threshold_noncoherent`, `det_pd_noncoherent` and `det_n_noncoh`
        had **zero** mentions in `native/tests/test_detection_core.c` — real
        coverage in `test_detection.py`, in the wrong language for an object
        whose header is the SSOT, and these three are exactly what
        `acq_core.c` sizes its (coherent depth, look count) split with.

    - **The header's headline number was unpinned.** `det_threshold_f` exists
        because a chi-square gate on an estimated noise reference *"realizes
        41x the priced pfa at n = 16"*, and nothing re-derived it. The
        behaviour is not a finding — a threshold built on a reference
        estimated from `n` samples inherits that estimate's uncertainty,
        which is why `det_threshold_f` exists — but the arithmetic executed
        nowhere, so the figure could drift from the code with no gate
        noticing. It is now re-derived in C, reproduced by Monte-Carlo
        (41.1x), and cross-checked against scipy, which also confirms
        `det_threshold_noncoherent(pfa, n/2)` *is* the chi2(n) quantile to
        1e-15 and `det_threshold_f` *is* the F(n,n) quantile.

        **More useful than the ratio: the number to budget.** 41x is a
        multiplier on the false-alarm *rate* and cannot be added to a link
        budget. What a caller needs is dB, indexed by how many samples they
        can spare for the reference — so §2.4 now derives and gates that
        table. The reference is known to `sqrt(2/n)`: 35% at 16 samples,
        14% at 100. Priced correctly that costs **3.27 dB at n = 16** and
        **0.97 dB at n = 100** (pfa 1e-3), more at a tighter false-alarm
        budget — 6.15 dB at 16 samples and pfa 1e-6. The penalty falls
        faster than `1/sqrt(n)` at small `n`, so the table is read rather
        than scaled, and the design rule is to fold in every prompt
        available rather than the minimum that reaches a decision.

        The comparator is derived rather than chosen to reproduce 41
        (`R^2 = n*F(n,n)` under H0; a caller treating `sum Im^2/n` as
        exactly `s^2` gates at the chi2(n) quantile, and since
        `det_threshold_noncoherent(pfa, M)` prices chi2(2M) that is
        `M = n/2`; `M = n` gives 4.8x, equally plausible on sight). The
        comparison is **even-`n` only** — `n/2` is integer division and
        doppler cannot price a known-noise gate at odd degrees of freedom
        at all, so at odd `n` it returns the chi2(n-1) gate and the
        multiplier comes out about a fifth high. Every site that
        reproduces it now asserts `n` is even instead of relying on its
        sweep to avoid the problem, which all three happened to do — which
        is exactly how the approximation stayed invisible.
        `det_threshold_f` carries no such restriction: a burst's prompt
        count is whatever the burst contained.

    - **`marcum_q`'s stated envelope was never exercised.** Every pinned
        value sat at `a <= 3` while the header promised `a, b <= 15`. Now
        measured against a Rice frequency at the edge. The sabotage is the
        point: dropping the series window's `sqrt(u)` scaling leaves **every
        pre-existing literal green** and takes only the new section red.

    - **`det_dwell_power` was declared twice** in the header, once with docs
        and once bare.

    **One finding is open, and it is not this object's to fix.**
    `acq_core.h` says twice that the non-coherent Pd model is
    "non-monotonic and unreliable past a few hundred looks" and bounds its
    search at `ACQ_N_NONCOH_SAFETY_CEILING = 256` on that basis. Measured:
    monotone out to **1024** looks, and within **0.2-0.6 sigma** of
    Monte-Carlo at 512, with H0 priced correctly there too. Neither half
    reproduces, and non-coherent looks are where the wideband mode buys all
    of its margin — so sensitivity may be sitting on the table
    ([#997](https://github.com/doppler-dsp/doppler/issues/997)).

- **`docs/design/detection.md`** — the page this module never had. Five
    statistical families ship under one `det_` prefix (Rayleigh envelope,
    chi-square non-coherent, exponential power, Gaussian CLT, and the F(n,n)
    ratio against an estimated noise reference), they are not
    interchangeable, and reaching for the wrong one does not fail loudly: at
    `pfa = 5e-6`, `det_threshold` returns 4.9409 and `det_q_inv` returns
    4.4172 — two plausible numbers near 5, only one of which is a sigma
    count. The page owns which law applies to which statistic, why
    non-coherent looks raise their own threshold (so `det_n_noncoh` must
    re-derive it every iteration), and what each helper errs toward when it
    is not exact.

- **`models` characterization subject** — the Monte-Carlo the
    tree was asserting rather than running. `acq_core.c` cites
    *"Validated against Monte-Carlo to \<1% (det_pd_noncoherent tests)"*; the
    sweep behind that sentence now exists, checking every family's H0 rate
    across four decades of `pfa` and both families' Pd curves against
    measured frequency, at 2 million draws per cell. Every cell agrees
    within 3 sigma, the envelope and power families come out **bit-identical**
    (not merely close), and the 1e-6 row is reported with its standard error
    rather than pretended to — at that draw count it expects two hits, so it
    is measuring the draw and says so.
