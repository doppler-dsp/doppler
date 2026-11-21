- **wfmgen's `ofdm` source takes the standard's 100 ns transition by
    default.** A source that gives no window (no `--window`, no `window`
    key, no `window=` keyword) renders with `2 * fs / bandwidth` samples, as
    `Wlan` does at the same oversampling, so its transmit mask passes where
    the rectangular window failed it when oversampled; `window=0` is the
    rectangular window by asking, and a C struct zero-filled by its caller is
    still rectangular. A scene or a SigMF record written from the source holds
    the count, `0` included. `dp_wfm_source_window` and `WFM_WINDOW_UNSET` are
    new, and `dp_wfm_source_bandwidth` names the 20 MHz default the bridge
    already used
    ([#3190](https://github.com/doppler-dsp/doppler/issues/3190)).
