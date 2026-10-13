- **The tile-edge alias's hop is pinned directly, not through a sweep.**
    `acq_resolve_tile_alias` is handed the row of the wrong neighbouring tile
    and must return the true one, so the guard no longer depends on the engine's
    raw pick landing in the neighbour (#2275).
