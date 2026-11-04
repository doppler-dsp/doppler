- **The Corr2D benchmark times a bank with Doppler rates and an
    interpolated delay-Doppler map.** `bank_64x512_c12_l4_r12_1t` runs the
    L = 4 bank through 12 per-period rate weightings, 144 hypotheses.
    `delay_doppler_32x1024_o64_1t` reads 32 periods on 64 Doppler rows. They
    are the two cases where the one delay-Doppler map changes the transform
    count, so main measures them before it lands (#2831).
