- **Two `mpsk_receiver_core.h` claims that nothing tested now have C tests**,
    both proven by sabotage ([#814](https://github.com/doppler-dsp/doppler/issues/814)).

    **§4b — the handover carries the frequency estimate across, both ways.** The
    header's claim is not that the flip happens (§4 pins that, along with the
    drop-back and the re-declare) but that *"the shared loop filter carries the
    frequency estimate across it in both directions, so a drop-back is a
    discriminator swap rather than a cold re-acquisition"*. §4 cannot test it, and
    not merely by omission — it calls `mpsk_receiver_set_norm_freq()` right after
    the drop-back, **overwriting the very quantity the claim is about**, so a
    receiver that cleared its filter on every mode change would pass §4 unchanged.

    §4b steps in one-symbol chunks so the measurement straddles each transition
    rather than bracketing it at block boundaries, and checks both: forward, the
    estimate is already the offset before the flip *and* undisturbed by it;
    reverse, the drop-back preserves it too, deliberately without §4's re-seed.
    The failure it guards is silent and expensive — a cold re-acquisition still
    reaches lock, so SER recovers and `tracking` returns to 1; it just pays the
    pull-in again, and at a marginal `bn` it slips instead.

    **§12 — the verify counts are time hysteresis.** *"Both directions are
    verify-counted (8 symbols up / 32 down)"* was documented and tested nowhere,
    and `carrier_nda`'s certification found the analogous count mattered a great
    deal — its `n_up = 8` false-declared 18/60 at one geometry. Measured as
    behaviour rather than by reading the count back: on one record, `n_up = 2`
    must declare strictly earlier than `n_up = 64`, and by at least the extra
    symbols asked for. Sabotage: making `configure_lock` ignore `n_up` takes both
    assertions red, where a count wired to nothing would give the same instant
    twice.
