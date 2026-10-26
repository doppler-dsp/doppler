- **just-makeit pin 0.111.0 → 0.111.2: an enum is importable from its
    package.** `doppler.dsss.ASYNC_DSSS_RX_STATE`, `doppler.spectral.PSD_WINDOW`,
    the wfm `STYPE`/`T0_SOURCE`/… tuples and their siblings now resolve
    without the submodule path, and `from doppler.wfm import *` binds every
    name `__all__` lists (0.111.1 re-exported them; 0.111.2 fixes the wfm
    reader's enums being listed but never imported, jm #2376).
