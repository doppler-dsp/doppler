- **`Acquisition(code_only_epochs, doppler_rate)` — the engine now allows a
    coherent depth inside every window tile, to accommodate waveforms with
    code-only windows (design §2.3).** The whole code-only epochs such a
    window holds size a depth `D = min((W+1)/2, f_epoch/sqrt(2·rate))`, run
    in non-overlapping `D`-epoch blocks per tile; the Doppler axis becomes
    one grid of `window_bins · D` bins of `chip_rate/(sf·D)` in FFT order,
    which `doppler_bin`, the hand-off and the surface axis all index. The
    block rides in the state blob (v3); the defaults are `D = 1` and the
    engine exactly as before. The hand-off's fold now runs over the combined
    bin count, which also makes it right for a burst engine's coherent axis.
