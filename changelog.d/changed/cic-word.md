- **A CIC uses its whole register, and its word is a setting.** `precision`
    16, 32 or 64 (the default: every bit left after the pipeline's gain)
    with `headroom_bits` above unity: full scale is now **4.0**, not 2.0,
    and the step 7e-12 at `R = 64`, not 6e-5. `precision = 16` is the old
    filter. `CIC_PAPR_HEADROOM` is gone ([budget](docs/design/cic.md)).
