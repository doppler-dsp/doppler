- **A general OFDM modulator.** `doppler.ofdm.Ofdm` turns constellation
    points and a layout (classes of OFDM symbol and runs of them: transform
    length, subcarrier map, pilot pattern with a polarity sequence, per-
    subcarrier gain, cyclic prefix, window) into baseband samples. It names no
    standard and needs no wfmgen in the process. `wlan`, the IEEE 802.11 builders, is its
    first caller; wfmgen's `ofdm` type is not built. Serializable, with the layout as its reject key. See the
    [guide](docs/guide/ofdm.md) and the [design](docs/design/ofdm.md).
