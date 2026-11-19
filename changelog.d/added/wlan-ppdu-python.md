- **`Wlan.ppdu()`: a whole 802.11 PPDU in Python.** `Wlan(constellation, code_rate, window=0,   oversample=1)` holds the window and the oversampling, and `ppdu(psdu, scrambler_state)`
    returns the `complex64` samples, held to the C output for the 36 Mb/s PPDU
    ([#3175](https://github.com/doppler-dsp/doppler/issues/3175)).
