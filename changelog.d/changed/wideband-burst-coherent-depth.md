- **A tiled (wideband) burst search keeps a coherent depth inside every Doppler
    tile when a design C/N0 asks for one; with none it stays at D = 1.** A tiled
    object built WITH a design C/N0 can now come out at D > 1 where 0.67.0 gave
    1 (45 dB-Hz: `coherent_bins` 1 to 8): its buffers, latency and state blob
    grow, and a 0.67.0 blob of it is refused on restore. A hit's `doppler_bin`
    indexes `window_bins * D` rows, which `BurstAcquisition.doppler_bins` now
    reports, as `BurstCapture` did (#2218, #2224; #2210).
