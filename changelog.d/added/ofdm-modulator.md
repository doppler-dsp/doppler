- **A general OFDM modulator.** `doppler.ofdm.Ofdm` turns constellation
    points and a layout (classes of OFDM symbol and runs of them: transform
    length, subcarrier map, pilot pattern with a polarity sequence, per-
    subcarrier gain, cyclic prefix, window) into baseband samples. It names no
    standard and needs no wfmgen in the process; the IEEE 802.11 presets and
    wfmgen's `ofdm` type are to be callers of it. Serializable, with the layout
    as its reject key ([design](docs/design/ofdm.md)).
