- **The named-args gate holds a parameter that is only compared with a gated
    enumerator.** `mode == DP_SPECTROGRAM_POWER`, a `!=`, or a `switch` on
    it makes the parameter a slot, so 7 more helpers are held, among them
    `rows_are_the_psd` and `oracle` again (#2227).
