- **`dp_interleave.h` — the block-interleaving permutation**, header-only like
    `dp_crc16.h` so neither the frame-stage kernel nor the coming `Interleaver`
    object grows a link-line dependency for arithmetic. Hard bits, octets and
    `float32` soft values, forward and inverse.
    [#1031](https://github.com/doppler-dsp/doppler/issues/1031)
