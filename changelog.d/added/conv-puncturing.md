- **Puncturing for the convolutional code.** `ConvEncoder` and `Viterbi` take
    `punct_period` and `punct_mask`, a pattern that deletes mother-code symbols
    to raise the rate; the decoder puts them back as zero-valued LLRs. The
    pattern and the position in it are in the state blob (v2), and a blob from
    another pattern is refused. Measured at equal Eb/N0: rate 1/2 < 2/3 < 3/4
    in error rate, all still ahead of uncoded BPSK at 3 dB
    ([design](docs/design/viterbi.md)). Node synchronization is not defined for
    a punctured code.
