- **A captured burst's event carries its Doppler rate.**
    `BurstCapture.events()` gains `doppler_rate_hz_s` (and
    `BurstCapture.doppler_rate_hz_s` holds the last event's): the rate, in
    Hz/s, of the best rate hypothesis over the whole preamble. At 23 dB over
    the design point it is the grid point nearest the true rate in every
    event (#2219).
