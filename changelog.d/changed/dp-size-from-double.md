- **A double is checked against a `size_t` in one place,
    `dp_size_from_double()`** in `clib_common.h`. It bounds against 2^w
    exactly, where `(double)SIZE_MAX` rounds up to 2^64 on LP64 and admits
    2^64 itself. wfmgen's sample-count flags use it, and
    `make lint-size-mul` now also fails on a new `(double)SIZE_MAX`.
