- **just-makeit pin 0.104.0 → 0.105.0.** An out-of-range `uint64_t` or
    `size_t` argument now raises `OverflowError`, a negative count no longer
    becomes `SIZE_MAX`, and numpy integers are accepted (jm gh-2220). The
    hand-owned fragments keep the masking parse (#2279).
