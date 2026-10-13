- **A sign slip in the tile-alias hop is now caught by `test_acq_core`.**
    A sweep of off-grid tones across the boundaries of a negative and a
    positive tile (D = 5) fails when either the hop frequency or the
    returned row of `acq_resolve_tile_alias` flips sign; the on-bin tones
    used before never reached that path (#2232).
