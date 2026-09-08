- **`Acquisition` exposes its complex intermediates** (design §12.21):
    `surface_complex()` is the last decided dwell's surface before the
    magnitude, amplitude and carrier phase per cell; `block_prompt(tile,   col, out)` is one cell's column of the last whole block, the per-epoch
    complex correlations at a code phase, the despread stream at epoch
    rate; `block_raw()` is the block's samples as pushed, for a
    re-correlation at any phase, rate or symbol boundary. Copies, in the
    engine's own sizes; 0 where the engine has none (a non-coherent dwell,
    a depth of one, a partial block). Pinned in `test_acq_core` and their
    doctests.
