- **One acquisition hypothesis with a declared Doppler rate now places swept
    bursts.** Refine searched rates only when the engine had several
    hypotheses, so a capped capture (every `DsssBurstReceiver` with a
    `max_rate`) named the wrong repetition for about 40% of the bursts its
    engine found: 0.555 against the engine's 0.946 at the design point. It
    now searches whenever a rate is declared, and delivers 0.896. It costs
    about 0.42 ms a burst. An event's `doppler_rate_hz_s` is 0 only without a
    declared rate (#2219).
