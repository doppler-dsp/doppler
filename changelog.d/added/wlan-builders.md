- **`wlan`: IEEE 802.11 non-HT OFDM builders.** A rate (a constellation and
    a code rate) and what follows from it (`doppler.wlan.Wlan`), the minimal
    MAC with its CRC-32 FCS, the scrambler, the punctured BCC and the layout
    for the OFDM modulator, in C. The unit test holds the RATE bits, both
    training sequences and the constellation encodings against the
    standard's printed form; what it does not hold, and the conformance
    claim, are in the [802.11 guide](docs/guide/wlan.md).
