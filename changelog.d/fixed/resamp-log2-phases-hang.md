- **`dp_resamp_create_custom` hung on a power-of-two `num_phases` above
    2^31**: its log2 shifted an `unsigned int` past its width and never
    stopped. It now shifts a `size_t`.
