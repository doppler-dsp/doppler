- **`min_gap`: the object derives the burst spacing a caller must leave.** The
    documented rule was `max(0, refine_span - burst_len)` — short by the whole
    detection-lag term, 32 samples against 528. It is now
    `refine_span + reps*code_period - burst_len`, derived from the claim rule
    and read back from `BurstCapture` and `DsssBurstReceiver`, so a caller
    applies no rule at all. Checked on four geometries, each predicting a
    different bound.
    Closes [#1172](https://github.com/doppler-dsp/doppler/issues/1172).
