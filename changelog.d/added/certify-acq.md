- **The `acq` engine is certified** —
    `src/doppler/dsss/tests/validation/acq/results.md`, 15 limits on every
    push, plus two C sections covering claims that had **zero mentions in
    either language**.

    **`samples_consumed` was pinned by nothing.** It is documented as a
    per-*hit* anchor precisely so a caller does not reuse one message-level
    timestamp across every detection in a push, and the failure it guards
    against is silent: stamping every hit with the same offset still detects
    in the right place, and only a caller correlating detections to
    wall-clock would ever see it. Now measured over a push spanning many
    epochs — every anchor distinct, strictly increasing, one dump's stride
    apart. Sabotage-proven by stamping a constant.

    **`noise_mode` was never exercised beyond its default**, on an engine
    whose constructor offers four CFAR references. `noise_est` is the
    denominator of the gating statistic, so the choice moves sensitivity by
    more than an order of magnitude: on one burst the statistic spans ~15 dB
    across the four, and `max` can legitimately suppress detection entirely.
    Asserted as an ordering — a property of the aggregation rather than of
    the draw — and sabotage-proven by discarding the configured mode at
    construction.

    **Two usability gaps filed rather than fixed**, both about the same
    question a caller asks first:

    - **A push shorter than one dwell returns nothing, and nothing says how
        long a dwell is** ([#999](https://github.com/doppler-dsp/doppler/issues/999)).
        The auto-sizer routinely picks `n_noncoh > 1`, so one dump needs
        several frames; a short push of a *perfect noiseless burst* yields
        zero hits in every mode. Read cold that says "the detector is
        broken" rather than "the dwell never completed". `test_acq_core.c`
        already carries a comment warning about it, which is the clearest
        evidence it is a real trap — and it caught this certification anyway,
        from Python, having read that comment.
    - **No property reports the searched Doppler reach, and
        `doppler_span_hz` reads as though it does**
        ([#998](https://github.com/doppler-dsp/doppler/issues/998)). That
        field is the *native* half-range `chip_rate/(2·sf)`, correctly
        documented and constant when window-tiling engages: at 4× the native
        span the engine really searches ±80 kHz while it still reports 16
        kHz. The reach is `doppler_bins · doppler_res_hz / 2` and has no
        accessor. This report's own coverage limit was written against
        `doppler_span_hz`, with the header open, and failed — the correct
        behaviour looked like a coverage bug for several minutes.

    Also certified: the two front doors choose genuinely different machinery
    (continuous window-tiles even with **no** uncertainty prior, closing the
    data-transition aliasing footgun structurally rather than pricing it as
    a tunable loss); a tighter Doppler prior lowers the per-cell threshold,
    so bounding the uncertainty buys **sensitivity** and not just runtime;
    `underpowered` is set when the link cannot meet the requested `pd` and
    clear when it can; `configure_search_raw` refuses out-of-range grids
    **and keeps the prior one**, so a refused reconfiguration cannot leave
    the threshold ladder mismatched to the cell count.
