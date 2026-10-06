- **`DopplerChannel.execute_profile(x, ppm)`: a Doppler PROFILE, one value per
    waveform sample** (doppler#940). The scalar `(doppler_ppm,   doppler_rate_ppm_s)` pair is a straight line and a satellite pass is not
    one (a 550 km overhead pass is +23.3 to -23.3 ppm and departs from its own
    best line by 21% of its range). The profile is absolute, parallel to the
    input, and its length is checked rather than trusted. It returns the same
    samples however the stream is chunked, bit for bit: the carrier is read
    off the position the resampler's own accumulator reports
    (`dp_resamp_execute_ctrl_pos()`), so no profile index is ever mapped to an
    output index, which is what made the first attempt chunk-dependent and
    got it reverted. Measured against the scalar closed form, the carrier
    differs by a constant `<= fc * 2**-32` Hz (0.58 Hz at 2.5 GHz): the
    resampler's 32-bit step, which the profile follows and the ideal does not.
    State blob layout version 2 (adds the profile's last `d`).
