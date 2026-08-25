- **`acq`: a burst at exactly half a coherent Doppler bin is detected again**
    ([#1002](https://github.com/doppler-dsp/doppler/issues/1002)). The
    slow-time FFT's scalloping loss is ~3.9 dB at the worst case — a peak
    landing exactly between two bins — and that was **not** a margin a caller
    could buy back with signal: `test_stat` saturates against the code's own
    autocorrelation-sidelobe floor, so a half-bin burst was invisible at *any*
    C/N0. Measured across a 12 dB sweep: zero detections every time.

    The engine now zero-pads its own column transform, sampling the surface
    between the native Doppler bins. Detection is flat across the bin
    (measured at 0, ¼, ½, ¾ and 1 bin) where it previously had a hole at ½.

    **Not via `corr2d`'s `ny_out`**, and the reason is worth recording:
    `corr2d` sees `acq`'s single-row reference (a code replica with no Doppler
    content of its own) and takes its fast path, in which the row axis
    provably cancels to an identity and is never transformed at all. Asking
    for `ny_out > ny` would force the general 2-D path and re-transform rows
    `acq` has already transformed. The Doppler axis belongs to `acq`, and
    padding its input is the textbook way to interpolate it.

    **The reported `doppler_bin` is unchanged** — still the native grid, so no
    consumer's `bin × doppler_res_hz` arithmetic moves. The peak that sets
    `peak_mag`/`test_stat` comes from the interpolated surface (that is where
    the sensitivity is recovered); the reported cell comes from a search
    restricted to native rows. Rounding an interpolated row to its nearer
    neighbour was tried first and cost ~5 points of Pd on the
    characterization's true-cell criterion, because a peak on an odd fine row
    reported the wrong neighbour.

    Two consequences a caller may notice: `acq_state_t` gains `n_surf` and
    `interp`, and the CFAR reference band now spans the surface rather than
    the native cell count. The threshold ladder is deliberately still sized
    from the **native** count — frequency-domain zero-padding is exact
    band-limited interpolation, so the added cells carry no new information
    and give the noise no extra independent chances. Verified: false-alarm
    rate measured over 80 000 frames is unchanged (1 hit before, 1 after).
