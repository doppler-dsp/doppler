- **`dp_correlator_state_t` no longer has `delay_doppler`.** It recorded
    the same fact as `flavor`; test
    `flavor == DP_CORRELATOR_DELAY_DOPPLER` instead
    ([#2901](https://github.com/doppler-dsp/doppler/issues/2901)).
