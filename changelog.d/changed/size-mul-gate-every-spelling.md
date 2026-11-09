- **`make lint-size-mul` reads every spelling of the guard**: SIZE_MAX
    written as `~(size_t)0`, `(size_t)~0` or `(size_t)-1`, in any numerator
    (`(SIZE_MAX - f) / h`), cast to any floating type, or compared in a
    floating clause, counted per occurrence. The eight sites it found in
    five files (buffer.h's framer ×2, psd ×2, interleaver ×2,
    dp_tlm_capture, util) moved onto `dp_size_mul` and the new
    `dp_size_add`.
