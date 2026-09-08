- **`Acquisition` exposes its complex intermediates, and the coherent
    discriminator on them is measured** (design §12.21): `surface_complex()`
    is the last decided dwell's surface before the magnitude, amplitude and
    carrier phase per cell; `block_prompt(tile, col, out)` is one cell's
    column of the last whole block, the per-epoch complex correlations at a
    code phase; `block_raw()` is the block's samples as pushed, for a
    re-correlation at any phase, rate or symbol boundary. Copies, in the
    engine's own sizes; 0 where the engine has none. On them
    `validate_acq_surface_jitter` reads the code phase at 0.012 chip per
    dwell in the window at 45 dB-Hz, beside the DLL's 0.013, and 0.031 under
    data where the magnitude read gave 0.085.
