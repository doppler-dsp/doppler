- **Two `mpsk_receiver_core.h` claims that nothing tested now have C tests**,
    both proven by sabotage ([#814](https://github.com/doppler-dsp/doppler/issues/814)).

    **§12 — the handover carries the frequency estimate across, both ways.** The
    header's claim is not that the flip happens (§4 pins that) but that *"the
    shared loop filter carries the frequency estimate across it in both
    directions, so a drop-back is a discriminator swap rather than a cold
    re-acquisition"*. §4 cannot test it: it re-seeds the carrier by hand across
    the outage, so it would pass against a receiver that cleared the estimate.
    §12 steps one sample at a time — the claim is about the instant of the flip,
    which a block call hides — and asserts the estimate is *continuous* there:
    within one loop update of its pre-flip value, and nowhere near the
    create-time seed. Both halves are load-bearing; the second is what stops the
    test passing on a loop that never moved. Sabotage: clearing the estimate at
    the flip takes both assertions red.

    **§13 — the verify counts are time hysteresis.** *"Both directions are
    verify-counted (8 symbols up / 32 down)"* was documented and tested nowhere,
    and `carrier_nda`'s certification found the analogous count mattered a great
    deal — its `n_up = 8` false-declared 18/60 at one geometry. Measured as
    behaviour rather than by reading the count back: on one record, `n_up = 2`
    must declare strictly earlier than `n_up = 64`, and by at least the extra
    symbols asked for. Sabotage: making `configure_lock` ignore `n_up` takes
    both assertions red, where a count wired to nothing would otherwise give the
    same instant twice.
