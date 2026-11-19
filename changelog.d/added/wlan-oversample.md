- **`wlan`: a PPDU at `M` times the 20 MHz rate.** `dp_wlan_layout()`,
    `dp_wlan_ppdu_len()` and `dp_wlan_ppdu()` take `oversample` (1 to 128), a
    larger transform rather than a resampler; `M = 1` is the PPDU as before, bit for
    bit ([#3175](https://github.com/doppler-dsp/doppler/issues/3175)).
