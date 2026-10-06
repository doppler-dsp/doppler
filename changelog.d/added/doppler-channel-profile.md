- **`DopplerChannel.execute_profile(x, ppm)`: a Doppler profile, one value per
    sample** (doppler#940). A pass is not the straight line the scalar pair
    describes (a 550 km overhead pass departs from its best line by 21%). The
    stream is bit-identical however it is chunked: the carrier is read off the
    resampler's own position, `dp_resamp_execute_ctrl_pos()`, not mapped from
    the profile. State blob layout version 2.
