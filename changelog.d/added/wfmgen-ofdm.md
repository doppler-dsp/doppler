- **`wfmgen --type ofdm`: 802.11a/g/p/j PPDUs from a data source.** One PPDU
    per frame, built through `wlan`: `wfmgen --type ofdm --wlan 54 --data ...`.
    `--wlan N` is the 20 MHz rate in Mb/s (or `--constellation` and
    `--code-rate`), `fs` is `M` times `--bandwidth` by a larger transform, and
    `--mac` is `none`, `fcs` or `data` (with `--mac-header`). The CLI, the
    scene JSON, the Python `Synth` and the C source reach it, and its SigMF
    annotation names its configuration. See the
    [guide](docs/guide/wfmgen/waveforms.md#ofdm-80211ag-ppdus)
    ([#3175](https://github.com/doppler-dsp/doppler/issues/3175)).
