- **wfm: with Doppler on, a burst's signal no longer continues through its
    gap** (doppler#1858). The channel was fed by the phase of the output being
    drained, so a refill fed a burst past its end (the gap carried it at full
    amplitude) and a burst after a delay could be lost entirely. The feed now
    follows the input timeline (`dp_wfm_render_set_input_timeline()`). Scenes
    with Doppler change; scenes without it are byte-identical.
