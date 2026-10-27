- **One Kaiser design rule, and no dead `MPSK_RX_EPS`.** resamp's built-in
    bank used private copies of `dp_kaiser_beta` and `dp_kaiser_num_taps`.
    The formulas now live once, header-inline in `resample_core.h`, and the
    public functions and resamp both call them; the bank is byte-identical.
    The unused `MPSK_RX_EPS` macro, a guard for a discriminator that no
    longer exists, is gone (#2652).
