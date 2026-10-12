- **A tiled (wideband) burst search keeps a coherent depth inside every
    Doppler tile when a design C/N0 asks for one; with none it stays at D = 1.**
    The sizer prices the tiles and the depth together in the threshold and the
    mean-noise reference, and interpolates each tile's slow-time axis. A hit's
    `doppler_bin` then indexes `window_bins * D` rows, and
    `BurstAcquisition.doppler_bins` now reports that product, as
    `BurstCapture.doppler_bins` already did, so
    `bin_to_signed(bin, doppler_bins) * doppler_res_hz` is the signed Doppler.
    The Pd model's coherent length for a tiled surface is #2224; per-dwell Pfa
    is #2218 (#2210).
