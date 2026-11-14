- **A tiled search with D > 1 now uses every thread asked for, not one a
    tile.** The pool was capped at the tile count, though the
    delay-Doppler map already fans by epoch, bin and row, and the decide
    pass now cuts each tile's rows into up to 8 equal chunks. At the #2457
    design point (3 tiles, D = 90) 16 threads went from 10–14 to 7.2–7.8
    ns a sample. The result is still bit-identical at any thread count;
    the CFAR reference's float rounding moves once, with the chunk count
    (#3047).
