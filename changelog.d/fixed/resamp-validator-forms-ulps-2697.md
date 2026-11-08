- **The resampler's validation report holds its two forms to 8 ULPs,
    not the bit.** At and above unity a zero control reproduces
    `execute` with the same count and each value within 8 ULPs, the
    rule the C test already pins (#2697); on the X Elite aarch64 build the
    bit-for-bit row read REGRESSED while every spectral-purity limit
    held.
