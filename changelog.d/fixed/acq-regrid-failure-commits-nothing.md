- **A failed acq regrid commits nothing.** The per-tile scratch is built with
    the other replacements and adopted at the one commit, so a plan that will
    not build leaves the engine on its old grid with nothing leaked, where it
    used to leave the new window and Doppler counts over the old surface, a
    stale `n`/`interp`/`n_surf`/`n_noncoh`, and a leaked ring and accumulator.
    Only the constructor reaches this today, so no caller saw it. See [#2761](https://github.com/doppler-dsp/doppler/issues/2761).
