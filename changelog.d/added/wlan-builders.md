- **`wlan`: IEEE 802.11 non-HT OFDM builders.** A rate (a constellation and
    a code rate) and what follows from it (`doppler.wlan.Wlan`), the minimal
    MAC with its CRC-32 FCS, the scrambler, the punctured BCC and the layout
    for the OFDM modulator, in C. The standard's literal tables are not in the
    tree yet, so a builder that needs one refuses and nothing claims
    conformance. See the [802.11 guide](docs/guide/wlan.md).
