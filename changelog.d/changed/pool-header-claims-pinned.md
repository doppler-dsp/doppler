- **`AsyncDsssPool`'s and `AsyncDsssReceiver`'s header claims are pinned
    in C, or corrected.** Every claim the certification inventory (#3267)
    found unasserted now has a C test with its failing case. Five sentences
    said more or less than the code does, and now say what it does:
    half a Doppler row is `chip_rate / (2 code_len D)`, not the full
    row; the 415.1-B-1 §7.4.1 pull-in is demonstrated where the record has
    the draws, and only not refuted at ±11 Hz/s; the search's pick count is
    one while no receiver is idle; a released ghost's cell is kept off by a
    rate, not a bound (#3263); and the carrier loop's figures are stated as
    31.2 / 29.8 Hz and 15.6 / 14.9 Hz instead of "about". No behavior
    changes.
