- **`DsssBurstReceiver`: a spurious detection no longer costs the next real
    burst** ([#1004](https://github.com/doppler-dsp/doppler/issues/1004)).
    The dedup rule armed `suppress_until = epoch + burst_len` on **every**
    detection, unconditionally and at detection time, and took the **first**
    hit in that window. So any spurious crossing — noise, or a neighbouring
    burst's payload firing against the acquisition code — blinded the search
    for a whole burst length, and the next real preamble was discarded before
    it was ever queued. On the 5-burst example capture that lost **2 of 5
    bursts**; the receiver now finds **5/5**, every one at its exact sample
    with a valid CRC.

    **The rule conflated two different jobs, and now does them separately.**

    - *Which detections are the same preamble* is an identity question, and
        two anchors name one burst exactly when refine can map both onto a
        single start. That reach is `refine_span`, so proximity within it is
        the test — and the **stronger** of the two keeps the slot, so a weak
        hit that merely arrived first cannot own it.
    - *Which detections are a burst's own payload* is only answerable once a
        burst has **decoded**. `suppress_until` is therefore armed on a valid
        frame and nowhere else, and candidates already queued inside the
        confirmed span go with it.

    **The tie-break is on `peak_mag`, deliberately not `test_stat`.** The
    gating statistic is peak over the CFAR noise estimate, and that estimate
    is a mean over the surface — so a *bare* preamble, which raises no floor,
    outscores a real burst whose payload does. Measured while building the
    regression test: a decoy at **0.35 amplitude** won the comparison on
    `test_stat` and lost it on `peak_mag`, which is the quantity the
    comparison actually means (how much preamble the frame holds).

    **The filed diagnosis was wrong, and was checked rather than inherited.**
    #1004 attributed the 3/5 to acquisition's non-overlapping framing
    splitting a straddled preamble. That hypothesis has one observable, and
    it does not track: the worst-straddled burst in the capture (67% single-
    frame coverage) was found while an 87%-covered one was lost, and sweeping
    the global frame phase moves coverage over 50%–100% — including an exact
    half-split — with the count pinned at 3/5 throughout. What does predict
    it, 1:1 across all nine phases, is the number of spurious detections:
    `found = 5 - suppressors`. Isolated by slicing the capture from inside a
    silent inter-burst gap at a frame-aligned offset, so each burst's straddle
    is bit-identical and only the preceding false alarm is removed: both lost
    bursts return, exact and valid.

    The genuine framing residual — a *single*-burst sweep near the knee, where
    no suppression is in play, finding 77% of offsets for an m-sequence
    against 6% for a structured code — is real, bounded to the knee, and split
    out as [#1006](https://github.com/doppler-dsp/doppler/issues/1006).

    Gated by `test_a_weak_decoy_does_not_cost_the_burst` in
    `native/tests/test_dsss_burst_receiver_core.c`, sabotage-proven on **both**
    halves independently: restoring the unconditional arm fails it, and
    dropping the greatest-of tie-break fails it. Note the suite could not have
    caught this before — with this file's short payload `burst_len` (2448) is
    *under* `refine_span` (2480), so the suppression window cannot reach past
    the burst it belongs to. The defect needs `burst_len > refine_span`, which
    is every realistic link (the example's ratio is 5.5x).

    `DSSS_BURST_RECEIVER_STATE_VERSION` is **2**: the queued-detection record
    carries the peak it is ranked on, so the blob layout moved.
