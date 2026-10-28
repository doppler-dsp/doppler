- **Meters report the right number in their corners.** `ADC.steps()`
    latches `clipped` on the same pre-rounding value as `step()`, and its
    TPDF dither takes two independent draws (the dither stream changes); the
    WFM writer and stream sink divide clips by the components quantised
    while tracking, so a real type no longer reads half and enabling it
    mid-stream measures only what follows; `BerMeter.score()` scores nothing
    without a successful alignment; `ber_theory_ber` is the chosen chance
    rate 0.5 for `esn0 <= 0`; `dp_ber_lock_symbol` returns `int64_t` (#2651).
