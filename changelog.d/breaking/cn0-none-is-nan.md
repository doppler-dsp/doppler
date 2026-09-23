- **A burst engine's "no design point" is NaN, not 0** (#1484).
    `cn0_dbhz` on `BurstAcquisition`, `BurstCapture`, `PersistentBurstCapture`,
    `DsssBurstReceiver` and `acq_create_burst*` now takes any finite value,
    negative included, and **NaN** (`ACQ_CN0_NONE` in C, and the new default)
    means none given. `0` used to mean none, which left normalized units
    (`fs = 1`, where C/N0 is the per-sample SNR, negative wherever acquisition
    is hard) no way to state a design point. **If you passed `cn0_dbhz=0` to
    mean "none", omit it (or pass NaN)**: `0` is now a real 0 dB-Hz design
    point and warns under-powered. Omitting the argument behaves exactly as
    before.
