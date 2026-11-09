- **`make lint-size-mul` reads every spelling of the guard**: SIZE_MAX
    written as `~(size_t)0` or `(size_t)-1`, in any numerator
    (`(SIZE_MAX - f) / h`), cast to any floating type, or compared in a
    floating clause. The six sites it found (buffer.h's framer, util, psd,
    interleaver, dp_tlm_capture) moved onto `dp_size_mul` and the new
    `dp_size_add`.
