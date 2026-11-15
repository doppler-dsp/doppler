- **A pool fed a sample rate whose code period is not a whole number of
    samples no longer re-seeds the signals it holds.** A tracking
    receiver's `chip_phase` was the phase at the last sample its live chain
    ran, not at the stream's next sample; the chain runs whole periods and
    carries the rest, so at 11 MSa/s on the 5 Mcps code it read about 50
    chips off. The pool builds its search's exclusions from that phase, so
    every held signal came back as a seed, was refined and released as a
    duplicate, and stopped the search while it refined (#3063).
