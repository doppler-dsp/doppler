- **just-makeit pin 0.110.0 → 0.111.0: each extension module publishes its
    enums' values.** `doppler.dsss.dsss.ASYNC_DSSS_RX_STATE` maps each
    receiver state's C value to its name, and `PSD_WINDOW`,
    `SPECTROGRAM_MODE`, the wfm `STYPE`/`T0_SOURCE`/… tuples and their
    siblings do the same, declared in the stubs (jm gh-2344).
    `Writer.annotate` loses an unreachable `None` branch (jm #2368).
