- **`costas`'s pull-in is measured now, not assumed** —
    `native/validation/costas_pullin.c`, the twin of `carrier_nda_pullin.c`
    that the async DSSS carrier loop never had.

    The gap it closes is a whole-object one. `carrier_nda` (the MPSK carrier
    discriminator) had four validators — pull-in, S-curve, lock, step
    response. `costas`, the loop an entire receiver's carrier recovery rests
    on, had **one**: `costas_jitter.c`. And the MPSK pull-in subject is
    explicit that jitter is the wrong thing to have on its own —

    > Tracking is not the question. Both loops track far beyond what they can
    > acquire; what is unpredictable is PULL-IN.

    So `costas` had exactly the measurement that subject calls not-the-question
    and none of the one it calls unpredictable.

    **What it pins.** The acquisition bound is `bn / m` (`m = 2`, the
    squaring discriminator), spelled through the shared
    `dp_test_freq_offset_inside_bw` rather than as a bare cycles/sample
    number. Asserted: acquires at and inside the bound on **both signs**, for
    a bare tone and for BPSK data; and the bound **scales with `bn`**, which
    is the law's actual content. Beyond the bound it reports a success
    fraction and asserts nothing, following `dp_sym_test.h`'s own rule that
    pull-in past the bound is "a characterization sweep with a reported
    success fraction, never a pass/fail assertion".

    **Measured, at the async receiver's own cadence** (bn 0.04 over a
    2046-sample code period → a 9.775e-6 cyc/sample bound, 60 Hz at SPEC's
    front end): 100% inside the bound on both signs, still 100% at 2x, and
    dead by 4x. Tighter than the MPSK carrier's 4x/5x envelope.

    **The symmetry section is a first-class check, not a note**, because
    [#982](https://github.com/doppler-dsp/doppler/issues/982) measured an
    asymmetry — a positive residual never failed out to +6x while a negative
    one failed from −2.5x. Nothing distinguishes +f0 from −f0 for a working
    loop. **It comes out symmetric**, so that asymmetry is not in this loop;
    #982 narrows to the chain around it.

    Sabotage-proven twice, both to exit 1: a loop a quarter as wide as it
    declares, and a synthetic sign asymmetry of #982's exact shape (which the
    symmetry section reports as `+100% / -0%`).

- **`async_dsss_receiver_core.h` no longer states its pull-in as prose.** The
    header said the refined seed leaves "~tens of Hz" of residual and sized
    `ASYNC_DSSS_RX_BN_CARRIER` against that. The residual is measured at
    **−54..+375 Hz** — 2–6x the loop's own bound — so the receiver asks its
    carrier loop to acquire from outside its range on most draws. The header
    now names the bound, points at the curve that re-derives it on every test
    run, and says not to widen `bn` again before checking the hand-off
    against it.
