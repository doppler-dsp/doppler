- **The searcher-timed tracker closed on its own cell, measured**
    (`validate_acq_surface_jitter`, design §12.23): acquired from the
    surface alone at the first window dwell, dead-reckoned on the held
    Doppler and corrected once a block by the coasting `Dll`'s read of
    the wiped raw block, the tracker holds the emitter at 0.014 chip at
    45 dB-Hz and 0.027 at 40 over 25 s, never leaving the cell where the
    surface's maximum was the emitter in only 58% of data dwells at the
    floor. The held phase carries the read's own noise; a filtered
    correction is the lever left. The `--check` is in the C suite.
