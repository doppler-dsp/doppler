- **A level problem is invisible to `MpskReceiver.lock`, and the report now
    says where to look instead.** With `agc = 0` the recovered symbol rate
    degrades from 17 to 172 ppm as the input level falls — the `A²`
    under-drive the header describes, arriving on the timing loop — while
    `lock` reads 0.96–0.97 throughout and is not even monotone in level,
    because `carrier_nda_disc` divides out its own `|z|^M` and is immune to
    the level by construction.

    So the receiver publishes two health readouts with disjoint blind spots,
    and the one a caller reaches for first is the blinder of the two.
    Diagnose with `agc_gain_db` and `timing_rate` instead. The gain law is
    exact: the readback plus `20·log10(amp)` is constant to under 0.01 dB
    across a 32× amplitude span, so the number is an absolute level estimate
    and not just a trend. Neither `lock` nor, at 20 dB Es/N0, the error rate
    can see a level error at all. Measured in the report's §2.9.
