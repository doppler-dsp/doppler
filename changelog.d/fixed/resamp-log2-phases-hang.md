- **`dp_resamp_create_custom` hung on a power-of-two `num_phases` above
    2^31**: its log2 shifted an `unsigned int` past its width and never
    stopped. It now shifts a `size_t`.
- **`dp_resamp_create_custom` accepted a `num_phases` above 2^32**, past
    which its arm select shifts the 32-bit phase word by a negative count
    (undefined). It is now refused, before the bank is copied.
