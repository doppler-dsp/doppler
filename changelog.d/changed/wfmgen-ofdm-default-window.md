- **wfmgen's `ofdm` source takes the standard's 100 ns transition by
    default.** With no `--window`, `window` key or `window=`, it renders
    `2 * fs / bandwidth` samples as `Wlan` does, so oversampled output meets
    the transmit mask; `window=0` is rectangular by asking. A scene or SigMF
    record holds the count; a scene written before this change for a
    rectangular window left the key out, and now loads as the standard's
    ([#3190](https://github.com/doppler-dsp/doppler/issues/3190)).
