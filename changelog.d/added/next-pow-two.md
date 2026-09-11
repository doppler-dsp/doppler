- **`next_pow_two` in `doppler.util`** — the transform-sizing primitive.
    Seven identical private copies were in the tree, under two spellings
    (`next_pow2` in `det_private.h`, `delay_core.c`, `psd_core.c`,
    `specan_core.c`, `ppe_core.c`; `*_pow2_ceil` in `burst_capture_core.c` and
    `dsss_burst_receiver_core.c`, the latter already dead). None guarded the
    overflow that makes a doubling loop spin forever; this one saturates to 0.
    Both spellings are now refused by the retired-names gate.
