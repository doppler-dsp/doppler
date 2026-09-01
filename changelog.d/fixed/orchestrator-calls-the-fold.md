- **The acquisition bank calls the library's Doppler fold instead of spelling
    it out.** `orchestrator._abs_doppler` carried its own copy of
    `bin_to_signed`. It agreed with the canonical form at every grid size —
    checked exhaustively for `n < 40` — which is exactly why it was worth
    removing: this arithmetic has already surfaced as a receiver reporting
    `tracking == 1` while decoding noise, when a search and its own hand-off
    spelled it differently.
    Closes [#1168](https://github.com/doppler-dsp/doppler/issues/1168).
