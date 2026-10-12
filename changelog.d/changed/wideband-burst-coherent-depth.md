- **A wideband burst search keeps its coherent depth inside every Doppler
    tile.** A `doppler_uncertainty` past the native span used to force a depth
    of 1 and discard the repetitions' gain; the sizer now prices the tiles and
    the depth together in the threshold and the mean-noise reference, and
    interpolates each tile's slow-time axis (#2210).
