- **`shl_q8`'s multiply is documented as deliberate**, with the numbers.
    Replacing C99's undefined negative left shift turned out to be a 4.5x
    speedup there — a packed 16-bit multiply vectorises eight lanes where the
    variable shift does not — so the obvious "repair" back to a shift is a
    2.9x regression. Comments only; the kernels are unchanged and verified
    exhaustively against an `int64_t` reference.
