- **The block searcher's per-cell passes run per tile (design §2.3, #1243).**
    The magnitude, the CFAR reference, the working mask and every scan of
    the peak list run on the engine's pool, one chunk of rows per tile,
    and merge serially in tile order; the block-end scatter reads a
    per-tile row table and the column gather goes 32 columns at a time.
    At the operating point (5 Mcps, ±50 kHz, D = 154) that is 624 → 523 ns
    per sample serially and 288 → 164 on four threads, byte-identical at
    any thread count. The CFAR mean over a tiled surface is now a mean of
    the tiles' means (equal cells), so its last bits can differ from
    before; a single-tile engine is unchanged to the bit.
