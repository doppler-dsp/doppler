- **The burst family's `cn0_dbhz` is a DESIGN C/N0, and optional.**
    `BurstAcquisition`, `BurstCapture`, `PersistentBurstCapture` and
    `DsssBurstReceiver` default it to 0 = none given: the search then
    integrates the whole preamble in one look with the threshold set by
    `pfa` alone; `pd` is a target only with a design point, and without
    one `pd_predicted` is NaN and `underpowered` never asserts. The old
    silent default of 50 dB-Hz is what sized the wrong grid.
    [#1181](https://github.com/doppler-dsp/doppler/issues/1181).
