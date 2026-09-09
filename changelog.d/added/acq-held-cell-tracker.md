- **The searcher-timed tracker closed on its own cell, measured**
    (`validate_acq_surface_jitter`, design §12.23–24): acquired from the
    surface alone, dead-reckoned on the held Doppler and corrected once
    a block by the coasting `Dll`'s read of the wiped raw block, it holds
    the emitter at the closed loop's jitter over 25 s and never leaves
    the cell where the surface's maximum was the emitter in 58% of data
    dwells at the floor; with the correction filtered at gain 1/8 it
    holds at 0.005 chip at 45 dB-Hz and 0.008 at 40, 2.5× under the
    loop, on a ~0.004 chip floor that is not the noise (source named,
    not measured). The `--check` is in the C suite.
